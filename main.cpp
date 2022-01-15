#include "audiorecorder.h"

#include <QtWidgets>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    AudioRecorder recorder;
    QFile fileDark(":styles.qss");
    fileDark.open(QFile::ReadOnly);
    app.setStyleSheet(fileDark.readAll());
    app.setWindowIcon(QIcon(":/img/LOGO.png"));
    recorder.setWindowTitle("Semnan Voice Recorder");
    recorder.show();

    return app.exec();
}
