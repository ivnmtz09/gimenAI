"""Contrato del panel Bluetooth con un ESP32 simulado, sin radio ni Gemini."""
import json
import unittest
from unittest.mock import patch

import httpx

from server import main


class BluetoothTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.nodes = patch.dict(main.esp32_nodes, {}, clear=True)
        self.nodes.start()
        self.client = httpx.AsyncClient(
            transport=httpx.ASGITransport(app=main.app), base_url="http://panel"
        )

    async def asyncTearDown(self):
        await self.client.aclose()
        self.nodes.stop()

    async def request_with_esp32(self, method, path, handler, **kwargs):
        main.esp32_nodes["test"] = {"ip": "192.0.2.1"}
        real_client = httpx.AsyncClient
        with patch.object(httpx, "AsyncClient", side_effect=lambda **opts: real_client(
            transport=httpx.MockTransport(handler), **opts
        )):
            return await self.client.request(method, path, **kwargs)

    async def test_no_esp32(self):
        response = await self.client.get("/api/bluetooth")
        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()["esp32"])
        self.assertEqual(response.json()["dispositivos"], [])

    async def test_all_commands_keep_esp32_available(self):
        for suffix, upstream in (("scan", "scan"), ("connect", "connect"), ("disconnect", "disconnect")):
            with self.subTest(command=suffix):
                def handler(request):
                    self.assertEqual(request.method, "POST")
                    self.assertEqual(request.url.path, "/api/bt/" + upstream)
                    if suffix == "connect":
                        self.assertEqual(json.loads(request.content)["address"], "AA:BB:CC:DD:EE:FF")
                    return httpx.Response(200, json={"dispositivos": [], "escaneando": suffix == "scan"})
                response = await self.request_with_esp32("POST", "/api/bluetooth/" + suffix, handler,
                    json={"address": "aa:bb:cc:dd:ee:ff"} if suffix == "connect" else None)
                self.assertEqual(response.status_code, 200)
                self.assertTrue(response.json()["esp32"])
                self.assertIn("conectado", response.json())

    async def test_status_returns_discovered_devices(self):
        devices = [{"nombre": "Parlante", "direccion": "AA:BB:CC:DD:EE:FF", "rssi": -50}]
        response = await self.request_with_esp32("GET", "/api/bluetooth", lambda req: httpx.Response(
            200, json={"dispositivos": devices, "conectado": True}))
        self.assertEqual(response.json()["dispositivos"], devices)
        self.assertTrue(response.json()["conectado"])

    async def test_invalid_addresses_never_reach_hardware(self):
        for address in ("", "invalid", "AA:BB:CC:DD:EE:FF\n", "AAA:BB:CC:DD:EE:FF"):
            with self.subTest(address=address):
                with patch.object(main, "esp32_request") as proxy:
                    response = await self.client.post("/api/bluetooth/connect", json={"address": address})
                    self.assertEqual(response.status_code, 400)
                    proxy.assert_not_called()

    async def test_older_firmware_reports_update_needed(self):
        response = await self.request_with_esp32("GET", "/api/bluetooth", lambda req: httpx.Response(404))
        self.assertEqual(response.status_code, 502)
        self.assertIn("firmware", response.json()["detail"])

    async def test_hardware_rejection_is_not_a_success(self):
        response = await self.request_with_esp32("POST", "/api/bluetooth/connect", lambda req: httpx.Response(
            409, json={"error": "Desconecta primero"}), json={"address": "AA:BB:CC:DD:EE:FF"})
        self.assertEqual(response.status_code, 409)
        self.assertEqual(response.json()["detail"], "Desconecta primero")

    async def test_invalid_status_is_not_a_success(self):
        for payload in ([], {"error": "Fallo"}):
            with self.subTest(payload=payload):
                response = await self.request_with_esp32("GET", "/api/bluetooth", lambda req: httpx.Response(200, json=payload))
                self.assertEqual(response.status_code, 502)

    async def test_timeout_clears_device_list(self):
        def handler(request):
            raise httpx.ReadTimeout("Sin respuesta", request=request)
        response = await self.request_with_esp32("GET", "/api/bluetooth", handler)
        self.assertFalse(response.json()["esp32"])
        self.assertEqual(response.json()["dispositivos"], [])


if __name__ == "__main__":
    unittest.main()
