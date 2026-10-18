#pragma once

// Estados principales de la Máquina de Estados Finitos (FSM)
enum RobotState {
    STATE_IDLE,         // Cara en reposo, pestañeo ocasional
    STATE_LISTENING,    // Cara atenta / orejas alertas, grabando audio
    STATE_PROCESSING,   // Indicador de "pensando" / procesando en la nube
    STATE_RESPONDING,   // Hablando (boca sincronizada), moviendo cabeza/brazos
    STATE_ERROR         // Indicador visual de error o desconexión
};

// Expresiones faciales que se dibujan en la pantalla ST7789
enum Expression {
    EXPR_NEUTRAL,       // Ojos normales abiertos
    EXPR_HAPPY,         // Ojos en arco feliz (^_^)
    EXPR_THINKING,      // Mirando hacia arriba o puntos suspensivos
    EXPR_TALKING,       // Boca abierta/cerrada animada
    EXPR_SURPRISED,     // Ojos abiertos grandes (O_O)
    EXPR_SAD,           // Ojos caídos / preocupación
    EXPR_SLEEPING       // Ojos cerrados (-_-)
};
