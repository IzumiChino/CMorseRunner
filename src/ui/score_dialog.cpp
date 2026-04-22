#include "score_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

ScoreDialog::ScoreDialog(int qso, int pts, int hiScore, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Contest Finished");
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    auto addRow = [&](const QString& label, int value) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(label, this));
        auto* val = new QLabel(QString::number(value), this);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(val);
        layout->addLayout(row);
    };

    addRow("QSOs:",      qso);
    addRow("Points:",    pts);
    addRow("Hi Score:",  hiScore);

    if (pts > hiScore) {
        auto* lbl = new QLabel("New High Score!", this);
        lbl->setAlignment(Qt::AlignCenter);
        QFont f = lbl->font();
        f.setBold(true);
        lbl->setFont(f);
        layout->addWidget(lbl);
    }

    auto* btn = new QPushButton("OK", this);
    connect(btn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(btn);

    adjustSize();
}
