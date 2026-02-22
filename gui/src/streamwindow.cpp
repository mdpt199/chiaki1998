// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <streamwindow.h>
#include <streamsession.h>
#include <loginpindialog.h>
#include <settings.h>
#include <QCoreApplication>

StreamWindow::StreamWindow(const StreamSessionConnectInfo &connect_info, QWidget *parent)
	: QMainWindow(parent),
	connect_info(connect_info)
{
	// Torna a janela um processo de fundo (sem ícone na barra de tarefas e sem bordas)
	setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);
	setAttribute(Qt::WA_DeleteOnClose);
	
	session = nullptr;
	av_widget = nullptr; 

	try {
		Init();
	}
	catch(const Exception &e) {
		close();
	}
}

void StreamWindow::Init()
{
	session = new StreamSession(connect_info, this);

	connect(session, &StreamSession::SessionQuit, this, &StreamWindow::SessionQuit);
	connect(session, &StreamSession::LoginPINRequested, this, &StreamWindow::LoginPINRequested);

	// Inicia a sessão de rede (essencial para o Ghost Zen)
	session->Start();

	// Garante que nenhuma janela seja exibida
	hide(); 
}

StreamWindow::~StreamWindow() {}

void StreamWindow::keyPressEvent(QKeyEvent *event) {
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::keyReleaseEvent(QKeyEvent *event) {
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::closeEvent(QCloseEvent *event) {
	if(session) {
		if(session->IsConnected() && connect_info.settings->GetDisconnectAction() == DisconnectAction::AlwaysSleep)
			session->GoToBed();
		session->Stop();
	}
}

void StreamWindow::SessionQuit(ChiakiQuitReason reason, const QString &reason_str) {
	close();
}

// Stubs para funções obrigatórias do Header
void StreamWindow::ToggleFullscreen() {}
void StreamWindow::UpdateVideoTransform() {}
void StreamWindow::LoginPINRequested(bool incorrect) { if(session) session->Stop(); }
