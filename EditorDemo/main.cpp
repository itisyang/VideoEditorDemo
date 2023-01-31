#include "editor_demo.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    EditorDemo w;
    w.show();
    return a.exec();
}
