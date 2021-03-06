#include "testapp.h"

#include <QDebug>
using namespace QtBackgroundProcess;

TestApp::TestApp(int &argc, char **argv) :
	App(argc, argv),
	statusTerm(nullptr)
{}

void TestApp::parseTerminalOptions()
{
	QCommandLineParser parser;
	setupParser(parser, true);
	parser.process(*this);
	setAutoStartMaster(parser.isSet(QStringLiteral("a")));
	setIgnoreMultiStarts(parser.isSet(QStringLiteral("i")));
}

int TestApp::startupApp(const QCommandLineParser &parser)
{
	doCommand(parser);
	qDebug() << "App Master started with arguments:"
			 << parser.positionalArguments()
			 << "and options:"
			 << parser.optionNames();

	connect(this, &TestApp::commandReceived,
			this, &TestApp::handleCommand);

	if(parser.isSet(QStringLiteral("m"))) {
		if(parser.value(QStringLiteral("m")) == QStringLiteral("echo")) {
			connect(this, &TestApp::newTerminalConnected,
					this, &TestApp::addTerminal);
			qDebug() << "Master started in echo mode!";
		} else if(parser.value(QStringLiteral("m")) == QStringLiteral("status")){
			statusTerm = new GlobalTerminal(this);
			qDebug() << "Master started in status mode!";
		} else if(parser.value(QStringLiteral("m")) == QStringLiteral("scream")){
			auto term = new GlobalTerminal(this);
			auto timer = new QTimer(this);
			timer->setInterval(500);
			qsrand(QDateTime::currentMSecsSinceEpoch());
			connect(timer, &QTimer::timeout, this, [term](){
				static const QByteArray strings="qwertzuiopasdfghjklyxcvbnm\n ";
				auto idx = (qrand() / (double)RAND_MAX) * (strings.size() - 1);
				term->write(strings.mid(idx, 1));
				term->flush();
			});
			timer->start();
			qDebug() << "Master started in scream mode!";
		} else if(parser.value(QStringLiteral("m")) == QStringLiteral("pid")) {
			qDebug() << "Master started in pid mode!";
			connect(this, &TestApp::newTerminalConnected, this, [this](Terminal *term){
				disconnect(this, &TestApp::newTerminalConnected,
						   this, nullptr);
				term->write(QByteArray::number(QCoreApplication::applicationPid()));
				term->flush();
				term->disconnectTerminal();
				qDebug() << "pid written";
			});
			auto term = new GlobalTerminal(this);
			term->writeLine(QByteArray::number(QCoreApplication::applicationPid()));
			term->flush();
		} else
			qWarning() << "Unknown mode! Will be ignored";
	}

	connect(this, &App::aboutToQuit, this, [](){
		qDebug() << "I am quitting!";
	});

	return EXIT_SUCCESS;
}

bool TestApp::requestAppShutdown(Terminal *terminal, int &)
{
	qDebug() << "stop requested with"
			 << terminal->parser()->positionalArguments()
			 << "and options:"
			 << terminal->parser()->optionNames();
	return true;
}

void TestApp::setupParser(QCommandLineParser &parser, bool useShortOptions)
{
	App::setupParser(parser, useShortOptions);

	parser.addOption({
						 {QStringLiteral("a"), QStringLiteral("autostart")},
						 QStringLiteral("Starts the master automatically, if not already running.")
					 });
	parser.addOption({
						 {QStringLiteral("i"), QStringLiteral("ignoreStart")},
						 QStringLiteral("If start is called a second time, the arguments will be omitted.")
					 });
	parser.addOption({
						 {QStringLiteral("f"), QStringLiteral("forward")},
						 QStringLiteral("forwards master debug output to all terminals if <active> = 1, disables it if <active> = 0."),
						 QStringLiteral("active"),
						 QStringLiteral("1")
					 });
	parser.addOption({
						 {QStringLiteral("m"), QStringLiteral("mode")},
						 QStringLiteral("Tells the master to run <mode>. Can be \"echo\" to simply echo all terminals, "
						 "\"status\" to simply broadcast new arguments to all terminals, \"scream\" to permanently "
						 "print stuff to all terminals, or \"pid\" to print the process id to all terminals. Unless explicitly set, nothing will be done"),
						 QStringLiteral("mode")
					 });
}

void TestApp::handleCommand(QSharedPointer<QCommandLineParser> parser, bool starter)
{
	if(starter) {
		qDebug() << "skipping starter args:"
				 << parser->positionalArguments()
				 << "and options:"
				 << parser->optionNames();
	} else {
		doCommand(*parser.data());
		qDebug() << "received new command:"
				 << parser->positionalArguments()
				 << "and options:"
				 << parser->optionNames();
	}

	if(statusTerm)
		statusTerm->writeLine('[' + parser->positionalArguments().join(QStringLiteral(", ")).toUtf8() + ']');
}

void TestApp::addTerminal(Terminal *terminal)
{
	//add a simple echo to all terminals
	connect(terminal, &Terminal::readyRead, terminal, [terminal](){
		terminal->write(terminal->readAll());
	});
}

void TestApp::doCommand(const QCommandLineParser &parser)
{
	if(parser.isSet(QStringLiteral("f")))
		setForwardMasterLog(parser.value(QStringLiteral("f")).toInt());
}

int main(int argc, char *argv[])
{
	TestApp a(argc, argv);
	TestApp::setApplicationVersion(QStringLiteral("4.2.0"));

	a.parseTerminalOptions();
	return a.exec();
}
