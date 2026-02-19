// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <streamwindow.h>
#include <streamsession.h>
#include <loginpindialog.h>
#include <settings.h>

#include <QLabel>
#include <QMessageBox>
#include <QCoreApplication>
#include <QAction>
#include <QVBoxLayout>

StreamWindow::StreamWindow(const StreamSessionConnectInfo &connect_info, QWidget *parent)
	: QMainWindow(parent),
	connect_info(connect_info)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(qApp->applicationName() + " | Controller (No Audio/Video)");
		
	session = nullptr;

	try
	{
		// Mantemos a opção de tela cheia caso queira capturar input exclusivo
		if(connect_info.fullscreen)
			showFullScreen();
		Init();
	}
	catch(const Exception &e)
	{
		QMessageBox::critical(this, tr("Init failed"), tr("Failed to initialize Session: %1").arg(e.what()));
		close();
	}
}

StreamWindow::~StreamWindow()
{
	// Removido delete av_widget pois ele não existe mais
}

void StreamWindow::Init()
{
	session = new StreamSession(connect_info, this);

	connect(session, &StreamSession::SessionQuit, this, &StreamWindow::SessionQuit);
	connect(session, &StreamSession::LoginPINRequested, this, &StreamWindow::LoginPINRequested);

	// Criamos um widget simples de fundo apenas para indicar o status
	QWidget *central_widget = new QWidget(this);
	central_widget->setStyleSheet("background-color: #222;");
	
	auto layout = new QVBoxLayout(central_widget);
	auto label = new QLabel(tr("Remote Session Active (Audio/Video Disabled)"), central_widget);
	label->setStyleSheet("color: white; font-weight: bold;");
	label->setAlignment(Qt::AlignCenter);
	layout->addWidget(label);
	
	setCentralWidget(central_widget);

	// Captura de teclado permanece ativa para enviar comandos ao console
	grabKeyboard();

	session->Start();

	// Atalho para fechar ou alternar tela
	auto quit_action = new QAction(tr("Quit"), this);
	quit_action->setShortcut(Qt::Key_Escape);
	addAction(quit_action);
	connect(quit_action, &QAction::triggered, this, &StreamWindow::close);

	resize(400, 200); // Tamanho reduzido, já que não há vídeo
	show();
}

// Os eventos de entrada são mantidos para que você ainda possa controlar o console
void StreamWindow::keyPressEvent(QKeyEvent *event)
{
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::keyReleaseEvent(QKeyEvent *event)
{
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::mousePressEvent(QMouseEvent *event)
{
	if(session) session->HandleMouseEvent(event);
}

void StreamWindow::mouseReleaseEvent(QMouseEvent *event)
{
	if(session) session->HandleMouseEvent(event);
}

void StreamWindow::closeEvent(QCloseEvent *event)
{
	if(session)
	{
		if(session->IsConnected())
		{
			// Lógica de Sleep ao desconectar preservada
			if(connect_info.settings->GetDisconnectAction() == DisconnectAction::AlwaysSleep)
				session->GoToBed();
		}
		session->Stop();
	}
}

void StreamWindow::SessionQuit(ChiakiQuitReason reason, const QString &reason_str)
{
	if(reason != CHIAKI_QUIT_REASON_STOPPED)
	{
		QMessageBox::warning(this, tr("Disconnected"), tr("Session quit unexpectedly."));
	}
	close();
}

void StreamWindow::LoginPINRequested(bool incorrect)
{
	auto dialog = new LoginPINDialog(incorrect, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
		grabKeyboard();
		if(!session) return;
		if(result == QDialog::Accepted)
			session->SetLoginPIN(dialog->GetPIN());
		else
			session->Stop();
	});
	releaseKeyboard();
	dialog->show();
}

// Removidas funções UpdateVideoTransform e referências ao PiDecoder/OpenGL
