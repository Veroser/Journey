#ifndef PARSERRUNNER_H
#define PARSERRUNNER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QDebug>
#include <QDate>

class ParserRunner : public QObject
{
    Q_OBJECT

public:
    explicit ParserRunner(QObject *parent = nullptr);
    ~ParserRunner() override;

    void runParser(const QString &scriptPath, const QString &jwtToken,
                   const QString &dbPath,
                   const QDate &startDate = QDate(), const QDate &endDate = QDate());
    bool isRunning() const;
    void cancelParser();

signals:
    void parserStarted();
    void parserFinished(bool success, const QString &message);
    void parserProgress(int percent);
    void parserError(const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    QProcess *m_process;
    QString m_pythonPath;

    bool findPython();
};

#endif // PARSERRUNNER_H