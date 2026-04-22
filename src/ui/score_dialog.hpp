#pragma once
#include <QDialog>
#include <QLabel>
#include <QPushButton>

class ScoreDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScoreDialog(int qso, int pts, int hiScore, QWidget* parent = nullptr);
};
