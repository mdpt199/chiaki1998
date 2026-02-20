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
#include <QKeyEvent>
#include <QMouseEvent>

StreamWindow::StreamWindow(const StreamSessionConnectInfo &connect_info, QWidget *parent)
	: QMainWindow(parent),
	connect_info(connect_info)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(qApp->applicationName() + " | Controller (No AV)");
		
	session = nullptr;
	av_widget = nullptr; // Mantido para compatibilidade com o .h

	try {
		Init();
	}
	catch(const Exception &e) {
		QMessageBox::critical(this, tr("Init failed"), e.what());
		close();
	}
}

StreamWindow::~StreamWindow() {
    // av_widget é nulo, então delete é seguro
}

void StreamWindow::Init()
{
	session = new StreamSession(connect_info, this);

	connect(session, &StreamSession::SessionQuit, this, &StreamWindow::SessionQuit);
	connect(session, &StreamSession::LoginPINRequested, this, &StreamWindow::LoginPINRequested);

	QWidget *central_widget = new QWidget(this);
	central_widget->setStyleSheet("background-color: black;");
	auto layout = new QVBoxLayout(central_widget);
	auto label = new QLabel("Remote Control Mode (No Video)", central_widget);
	label->setStyleSheet("color: white;");
	label->setAlignment(Qt::AlignCenter);
	layout->addWidget(label);
	setCentralWidget(central_widget);

	grabKeyboard();
	session->Start();

	resize(640, 480);
	show();
}

// Reimplementação das funções para satisfazer o Linker (satisfaz o streamwindow.h)

void StreamWindow::ToggleFullscreen() { 
    // Vazio ou logica simples
    if(isFullScreen()) showNormal(); else showFullScreen();
}

void StreamWindow::keyPressEvent(QKeyEvent *event) {
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::keyReleaseEvent(QKeyEvent *event) {
	if(session) session->HandleKeyboardEvent(event);
}

void StreamWindow::mousePressEvent(QMouseEvent *event) {
	if(session) session->HandleMouseEvent(event);
}

void StreamWindow::mouseReleaseEvent(QMouseEvent *event) {
	if(session) session->HandleMouseEvent(event);
}

void StreamWindow::mouseDoubleClickEvent(QMouseEvent *event) {
	ToggleFullscreen();
	QMainWindow::mouseDoubleClickEvent(event);
}

void StreamWindow::resizeEvent(QResizeEvent *event) {
	QMainWindow::resizeEvent(event);
}

void StreamWindow::moveEvent(QMoveEvent *event) {
	QMainWindow::moveEvent(event);
}

void StreamWindow::changeEvent(QEvent *event) {
	QMainWindow::changeEvent(event);
}

void StreamWindow::UpdateVideoTransform() {
    // Vazio
}

void StreamWindow::closeEvent(QCloseEvent *event)
{
	if(session) {
		if(session->IsConnected() && connect_info.settings->GetDisconnectAction() == DisconnectAction::AlwaysSleep)
			session->GoToBed();
		session->Stop();
	}
}

void StreamWindow::SessionQuit(ChiakiQuitReason reason, const QString &reason_str) {
	close();
}

void StreamWindow::LoginPINRequested(bool incorrect) {
	auto dialog = new LoginPINDialog(incorrect, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
		grabKeyboard();
		if(!session) return;
		if(result == QDialog::Accepted) session->SetLoginPIN(dialog->GetPIN());
		else session->Stop();
	});
	releaseKeyboard();
	dialog->show();
}
