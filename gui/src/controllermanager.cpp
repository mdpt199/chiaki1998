// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <controllermanager.h>
#include <QCoreApplication>
#include <QMessageBox>
#include <QByteArray>
#include <QTimer>
#include <QSettings> 

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

// --- Estrutura e Variáveis Globais para o Anti-Recoil ---
struct RecoilProfile {
    int vertical = 0;   
    int horizontal = 0; 
    bool enabled = false;
};

static RecoilProfile currentRecoil; 

static QSet<QString> chiaki_motion_controller_guids({
	"03000000341a00003608000011010000",
    "050000004c050000cc090000ffff3f00",
    "3J5TqBf8fb7w58vA6mJ7KBtqLjmtQaD3QY",
	"030000008f0e00001431000000000000",
});

static ControllerManager *instance = nullptr;
#define UPDATE_INTERVAL_MS 4

// --- Implementação do ControllerManager ---

ControllerManager *ControllerManager::GetInstance()
{
	if(!instance)
		instance = new ControllerManager(qApp);
	return instance;
}

ControllerManager::ControllerManager(QObject *parent)
	: QObject(parent)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_SetMainReady();
	if(SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
	{
		const char *err = SDL_GetError();
		QMessageBox::critical(nullptr, "SDL Init", tr("Failed to initialized SDL: %1").arg(err ? err : ""));
	}

	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &ControllerManager::HandleEvents);
	timer->start(UPDATE_INTERVAL_MS);
#endif
    
    // Carrega configurações via QSettings diretamente
    QSettings s;
    currentRecoil.vertical = s.value("recoil/vertical", 0).toInt();
    currentRecoil.horizontal = s.value("recoil/horizontal", 0).toInt();
    currentRecoil.enabled = s.value("recoil/enabled", false).toBool();

	UpdateAvailableControllers();
}

ControllerManager::~ControllerManager()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_Quit();
#endif
}

void ControllerManager::UpdateAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	QSet<SDL_JoystickID> current_controllers;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(!SDL_IsGameController(i))
			continue;

		current_controllers.insert(SDL_JoystickGetDeviceInstanceID(i));
	}

	if(current_controllers != available_controllers)
	{
		available_controllers = current_controllers;
		emit AvailableControllersUpdated();
	}
#endif
}

void ControllerManager::HandleEvents()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				UpdateAvailableControllers();
				break;
			case SDL_CONTROLLERBUTTONUP:
			case SDL_CONTROLLERBUTTONDOWN:
				ControllerEvent(event.cbutton.which);
				break;
			case SDL_CONTROLLERAXISMOTION:
				ControllerEvent(event.caxis.which);
				break;
		}
	}
#endif
}

void ControllerManager::ControllerEvent(int device_id)
{
	if(!open_controllers.contains(device_id))
		return;
	open_controllers[device_id]->UpdateState();
}

QSet<int> ControllerManager::GetAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return available_controllers;
#else
	return {};
#endif
}

Controller *ControllerManager::OpenController(int device_id)
{
	if(open_controllers.contains(device_id))
		return nullptr;
	auto controller = new Controller(device_id, this);
	open_controllers[device_id] = controller;
	return controller;
}

void ControllerManager::ControllerClosed(Controller *controller)
{
	open_controllers.remove(controller->GetDeviceID());
}

// --- Implementação do Controller ---

Controller::Controller(int device_id, ControllerManager *manager) : QObject(manager)
{
	this->id = device_id;
	this->manager = manager;

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	controller = nullptr;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(SDL_JoystickGetDeviceInstanceID(i) == device_id)
		{
			controller = SDL_GameControllerOpen(i);
			break;
		}
	}
#endif
}

Controller::~Controller()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(controller)
		SDL_GameControllerClose(controller);
#endif
	manager->ControllerClosed(this);
}

void Controller::UpdateState()
{
	emit StateChanged();
}

bool Controller::IsConnected()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return controller && SDL_GameControllerGetAttached(controller);
#else
	return false;
#endif
}

int Controller::GetDeviceID()
{
	return id;
}

QString Controller::GetName()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
	return QString(SDL_JoystickName(js));
#else
	return QString();
#endif
}

ChiakiControllerState Controller::GetState()
{
	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return state;

    // Mapeamento original
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

    int16_t r2_raw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    int16_t rx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

    // Lógica de Anti-Recoil (Compensação)
    if (currentRecoil.enabled && r2_raw > 1000) {
        int32_t comp_y = ry + currentRecoil.vertical;
        int32_t comp_x = rx + currentRecoil.horizontal;
        ry = (int16_t)qBound(-32768, comp_y, 32767);
        rx = (int16_t)qBound(-32768, comp_x, 32767);
    }

	state.l2_state = (uint8_t)(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
	state.r2_state = (uint8_t)(r2_raw >> 7);
	state.left_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
	state.left_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
	state.right_x = rx;
	state.right_y = ry;
#endif
	return state;
}

void Controller::SetRumble(uint8_t left, uint8_t right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller) return;
	SDL_GameControllerRumble(controller, (uint16_t)left << 8, (uint16_t)right << 8, 5000);
#endif
}
