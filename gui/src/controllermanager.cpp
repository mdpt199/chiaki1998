// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <controllermanager.h>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>
#include <QSettings> 

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

struct RecoilProfile {
    int vertical = 0;   
    int horizontal = 0; 
    bool enabled = false;
};

static RecoilProfile currentRecoil; 

// Identificadores de Controles (Sony/DualSense)
static QSet<QString> chiaki_motion_controller_guids({
    "030000004c0500006802000010010000",
    "050000004c050000cc090000ffff3f00",
    "030000004c050000cc09000000000000"
});

static ControllerManager *instance = nullptr;

ControllerManager *ControllerManager::GetInstance() {
    if(!instance) instance = new ControllerManager(qApp);
    return instance;
}

ControllerManager::ControllerManager(QObject *parent) : QObject(parent) {
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ControllerManager::HandleEvents);
    timer->start(4);
#endif
    // Carrega Recoil do Registro do Windows
    QSettings s;
    currentRecoil.vertical = s.value("recoil/vertical", 0).toInt();
    currentRecoil.horizontal = s.value("recoil/horizontal", 0).toInt();
    currentRecoil.enabled = s.value("recoil/enabled", false).toBool();
    UpdateAvailableControllers();
}

ControllerManager::~ControllerManager() {
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    SDL_Quit();
#endif
}

// --- Lógica de Captura e Compensação ---
ChiakiControllerState Controller::GetState() {
    ChiakiControllerState state;
    chiaki_controller_state_set_idle(&state);
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    if(!controller) return state;

    // Mapeamento básico (Resumido para o exemplo)
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) ? CHIAKI_CONTROLLER_BUTTON_CROSS : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? CHIAKI_CONTROLLER_BUTTON_R1 : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_GUIDE) ? CHIAKI_CONTROLLER_BUTTON_PS : 0;

    int16_t r2_raw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    int16_t rx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

    // ANTI-RECOIL: Puxa o analógico se R2 estiver pressionado
    if (currentRecoil.enabled && r2_raw > 1000) {
        ry = (int16_t)qBound(-32768, (int32_t)ry + currentRecoil.vertical, 32767);
        rx = (int16_t)qBound(-32768, (int32_t)rx + currentRecoil.horizontal, 32767);
    }

    state.r2_state = (uint8_t)(r2_raw >> 7);
    state.right_x = rx;
    state.right_y = ry;
    state.left_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    state.left_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
#endif
    return state;
}
// (Restante das funções padrão do ControllerManager omitidas...)
