// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <controllermanager.h>
#include <QCoreApplication>
#include <QMessageBox>
#include <QByteArray>
#include <QTimer>
#include <QSettings> // Necessário para salvar os perfis

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

// --- Estrutura para o Anti-Recoil ---
struct RecoilProfile {
    int vertical = 0;   // Intensidade de puxada para baixo
    int horizontal = 0; // Ajuste lateral (drift)
    bool enabled = false;
};

// Perfil global ativo (pode ser expandido para uma lista/mapa de perfis)
static RecoilProfile currentRecoil; 

static QSet<QString> chiaki_motion_controller_guids({
    "03000000341a00003608000011010000",
    "030000004c0500006802000010010000",
    // ... (restante das GUIDs omitidas para brevidade, mantenha as originais aqui)
    "030000008f0e00001431000000000000",
});

static ControllerManager *instance = nullptr;
#define UPDATE_INTERVAL_MS 4

ControllerManager *ControllerManager::GetInstance() {
    if(!instance)
        instance = new ControllerManager(qApp);
    return instance;
}

ControllerManager::ControllerManager(QObject *parent) : QObject(parent) {
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        const char *err = SDL_GetError();
        QMessageBox::critical(nullptr, "SDL Init", tr("Failed to initialized SDL: %1").arg(err ? err : ""));
    }

    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ControllerManager::HandleEvents);
    timer->start(UPDATE_INTERVAL_MS);
#endif
    LoadSettings(); // Carrega o perfil salvo ao iniciar
    UpdateAvailableControllers();
}

// Métodos para persistência de dados
void ControllerManager::LoadSettings() {
    QSettings s;
    currentRecoil.vertical = s.value("recoil/vertical", 0).toInt();
    currentRecoil.horizontal = s.value("recoil/horizontal", 0).toInt();
    currentRecoil.enabled = s.value("recoil/enabled", false).toBool();
}

void ControllerManager::SaveSettings() {
    QSettings s;
    s.setValue("recoil/vertical", currentRecoil.vertical);
    s.setValue("recoil/horizontal", currentRecoil.horizontal);
    s.setValue("recoil/enabled", currentRecoil.enabled);
}

// ... (UpdateAvailableControllers, HandleEvents e ControllerEvent permanecem iguais)

ChiakiControllerState Controller::GetState()
{
    ChiakiControllerState state;
    chiaki_controller_state_set_idle(&state);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    if(!controller)
        return state;

    // 1. Mapeamento de Botões Padrão
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) ? CHIAKI_CONTROLLER_BUTTON_CROSS : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) ? CHIAKI_CONTROLLER_BUTTON_MOON : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X) ? CHIAKI_CONTROLLER_BUTTON_BOX : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) ? CHIAKI_CONTROLLER_BUTTON_PYRAMID : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) ? CHIAKI_CONTROLLER_BUTTON_DPAD_UP : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ? CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? CHIAKI_CONTROLLER_BUTTON_L1 : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? CHIAKI_CONTROLLER_BUTTON_R1 : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK) ? CHIAKI_CONTROLLER_BUTTON_L3 : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) ? CHIAKI_CONTROLLER_BUTTON_R3 : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START) ? CHIAKI_CONTROLLER_BUTTON_OPTIONS : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK) ? CHIAKI_CONTROLLER_BUTTON_SHARE : 0;
    state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_GUIDE) ? CHIAKI_CONTROLLER_BUTTON_PS : 0;

    // 2. Leitura dos Gatilhos e Eixos
    int16_t l2_raw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    int16_t r2_raw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    
    int16_t rx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

    // 3. Lógica de Anti-Recoil (Compensação)
    // Ativa apenas se R2 estiver pressionado (atirar) e a função estiver ligada
    if (currentRecoil.enabled && r2_raw > 1000) {
        // Soma a intensidade ao eixo Y. No SDL/Chiaki, valores positivos puxam para BAIXO.
        int32_t compensated_y = ry + currentRecoil.vertical;
        int32_t compensated_x = rx + currentRecoil.horizontal;

        // Garante que o valor não ultrapasse os limites do analógico (-32768 a 32767)
        ry = (int16_t)qBound(-32768, compensated_y, 32767);
        rx = (int16_t)qBound(-32768, compensated_x, 32767);
    }

    state.l2_state = (uint8_t)(l2_raw >> 7);
    state.r2_state = (uint8_t)(r2_raw >> 7);
    state.left_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    state.left_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    state.right_x = rx;
    state.right_y = ry;

#endif
    return state;
}

// ... (Restante do arquivo ControllerManager/Controller preservado)
