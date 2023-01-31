#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_editordemo.h"

class EditorDemo : public QMainWindow
{
    Q_OBJECT

public:
    EditorDemo(QWidget *parent = nullptr);
    ~EditorDemo();

private:
    Ui::EditorDemoClass ui;
};
