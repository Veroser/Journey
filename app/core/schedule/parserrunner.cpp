#include "ParserRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

ParserRunner::ParserRunner(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
{
    // Больше не ищем Python — используем свои бинарники
}

ParserRunner::~ParserRunner()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(3000);
        delete m_process;
    }
}

QString ParserRunner::binaryPath(const QString &scriptName) const
{
    // scriptName: "schedule.py", "homework.py", "performance.py"
    // Возвращает путь к бинарнику в db/dist/
    QFileInfo scriptInfo(scriptName);
    QString baseName = scriptInfo.completeBaseName(); // "schedule" без .py

    QString distDir = QCoreApplication::applicationDirPath() + "/db/dist";
    QString binary = distDir + "/" + baseName;

    // На Windows добавляем .exe
#ifdef Q_OS_WIN
    binary += ".exe";
#endif

    return binary;
}

void ParserRunner::runParser(const QString &scriptPath, const QString &jwtToken,
                             const QString &dbPath,
                             const QDate &startDate, const QDate &endDate)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit parserError("Парсер уже запущен");
        return;
    }

    // Определяем путь к бинарнику
    QString executable = binaryPath(scriptPath);

    // Проверяем существование бинарника
    if (!QFileInfo::exists(executable)) {
        emit parserError("Бинарник парсера не найден: " + executable);
        return;
    }

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ParserRunner::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &ParserRunner::onProcessError);
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &ParserRunner::onReadyReadStandardOutput);
        connect(m_process, &QProcess::readyReadStandardError,
                this, &ParserRunner::onReadyReadStandardError);
    }

    // Аргументы для бинарника
    QStringList args;
    args << "--token" << jwtToken;
    args << "--db" << dbPath;

    if (startDate.isValid()) {
        args << "--start-date" << startDate.toString("yyyy-MM-dd");
    }
    if (endDate.isValid()) {
        args << "--end-date" << endDate.toString("yyyy-MM-dd");
    } else if (startDate.isValid()) {
        args << "--end-date" << startDate.addMonths(3).toString("yyyy-MM-dd");
    }

    qDebug() << "Запуск парсера:" << executable << args;

    m_process->setProgram(executable);
    m_process->setArguments(args);
    m_process->start();

    if (m_process->waitForStarted(5000)) {
        emit parserStarted();
        qDebug() << "Парсер запущен:" << executable;
    } else {
        emit parserError("Не удалось запустить парсер: " + m_process->errorString());
    }
}

bool ParserRunner::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void ParserRunner::cancelParser()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
        qDebug() << "Парсер остановлен";
        emit parserFinished(false, "Парсинг отменен пользователем");
    }
}

void ParserRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    if (exitCode == 0) {
        qDebug() << "Парсер успешно завершил работу";
        emit parserFinished(true, "Синхронизация успешно завершена");
    } else {
        QString errorMsg = m_process->readAllStandardError();
        qWarning() << "Парсер завершился с ошибкой:" << exitCode;
        qWarning() << "Ошибка:" << errorMsg;
        emit parserFinished(false, "Ошибка синхронизации. Код: " + QString::number(exitCode));
    }
}

void ParserRunner::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Не удалось запустить парсер. Проверьте наличие бинарников в db/dist/";
        break;
    case QProcess::Crashed:
        errorMsg = "Парсер аварийно завершился.";
        break;
    case QProcess::Timedout:
        errorMsg = "Парсер превысил время выполнения.";
        break;
    default:
        errorMsg = "Неизвестная ошибка парсера: " + m_process->errorString();
    }

    qWarning() << "Ошибка процесса:" << errorMsg;
    emit parserError(errorMsg);
}

void ParserRunner::onReadyReadStandardOutput()
{
    QString output = m_process->readAllStandardOutput();
    qDebug() << "Парсер:" << output.trimmed();

    if (output.contains("PROGRESS:")) {
        QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            if (line.startsWith("PROGRESS:")) {
                bool ok;
                int percent = line.mid(9).trimmed().toInt(&ok);
                if (ok) {
                    emit parserProgress(percent);
                }
            }
        }
    }
}

void ParserRunner::onReadyReadStandardError()
{
    QString error = m_process->readAllStandardError();
    if (!error.trimmed().isEmpty()) {
        qDebug() << "Парсер (stderr):" << error.trimmed();
    }
}