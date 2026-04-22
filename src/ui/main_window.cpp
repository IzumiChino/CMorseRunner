#include "main_window.hpp"
#include "score_dialog.hpp"
#include "../core/ini.hpp"
#include "../core/log.hpp"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QMenuBar>
#include <QMenu>
#include <QShortcut>
#include <QKeyEvent>
#include <QEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QFont>
#include <QColor>
#include <QLabel>
#include <QFrame>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QContextMenuEvent>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

// ── helpers ──────────────────────────────────────────────────────────────────

static QFont monoFont(int pt = 0)
{
	QFont f;
	f.setFamily("Monospace");
	f.setStyleHint(QFont::Monospace);
	if (pt > 0)
		f.setPointSize(pt);
	return f;
}

static QString macroButtonLabel(int index, const std::string &text)
{
	QString body = QString::fromStdString(text);
	body.replace("$MYCALL", "MY");
	body.replace("$HIS", "HIS");
	body.replace("$EXCH", "EXCH");
	body = body.simplified();
	if (body.size() > 14)
		body = body.left(14) + "...";
	return QString("F%1 %2").arg(index + 1).arg(body);
}

// ── ctor / dtor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setupMenuBar();
	setupCentralWidget();
	setupShortcuts();
	syncMenuToIni();

	contest_.onScoreChanged = [this](int q, int p) {
		QMetaObject::invokeMethod(
			this,
			[this, q, p] { onScoreChanged(q, p); },
			Qt::QueuedConnection);
	};
	contest_.onTimerTick = [this](double t) {
		QMetaObject::invokeMethod(
			this,
			[this, t] { onTimerTick(t); },
			Qt::QueuedConnection);
	};
	contest_.onCallDecoded = [this](const std::string &s) {
		QString qs = QString::fromStdString(s);
		QMetaObject::invokeMethod(
			this,
			[this, qs] { onCallDecoded(qs); },
			Qt::QueuedConnection);
	};
	contest_.onQsoLogged = [this](const Qso &q) {
		Qso copy = q;
		QMetaObject::invokeMethod(
			this,
			[this, copy] { applyQsoRow(copy); },
			Qt::QueuedConnection);
	};
	contest_.onDurationExpired = [this]() {
		QMetaObject::invokeMethod(
			this,
			[this] {
				audio_.stop();
				contest_.stop();
				running_ = false;
				lblMode_->setText("--");
				actRunPileup_->setChecked(false);
				actRunSingle_->setChecked(false);
				auto &ini = Ini::instance();
				if (contest_.points > ini.hiScore)
					ini.hiScore = contest_.points;
				ScoreDialog dlg(contest_.qsoCount,
					contest_.points,
					ini.hiScore,
					this);
				dlg.exec();
			},
			Qt::QueuedConnection);
	};

	syncLeftPanelToIni();
	updateMacroLabels();
	setWindowTitle("CMorseRunner");
	resize(1060, 660);
}

MainWindow::~MainWindow()
{
	audio_.stop();
	contest_.stop();
	Ini::instance().save("morserunner.ini");
}

// ── menu bar ──────────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
	auto &ini = Ini::instance();

	// ── File ──
	auto *mFile = menuBar()->addMenu(tr("&File"));
	actRecording_ = mFile->addAction(tr("Audio &Recording"));
	actRecording_->setCheckable(true);
	actRecording_->setChecked(ini.saveWav);
	connect(actRecording_, &QAction::toggled, [](bool v) {
		Ini::instance().saveWav = v;
	});
	mFile->addSeparator();
	auto *actExit =
		mFile->addAction(tr("E&xit"), qApp, &QApplication::quit);
	actExit->setShortcut(QKeySequence::Quit);

	// ── Run ──
	auto *mRun = menuBar()->addMenu(tr("&Run"));
	runGroup_ = new QActionGroup(this);
	actRunPileup_ = mRun->addAction(tr("&Pile-Up"));
	actRunPileup_->setCheckable(true);
	runGroup_->addAction(actRunPileup_);
	actRunSingle_ = mRun->addAction(tr("&Single Calls"));
	actRunSingle_->setCheckable(true);
	runGroup_->addAction(actRunSingle_);
	mRun->addSeparator();
	actRunStop_ =
		mRun->addAction(tr("S&top"), this, &MainWindow::onRunStop);
	connect(actRunPileup_,
		&QAction::triggered,
		this,
		&MainWindow::onRunPileup);
	connect(actRunSingle_,
		&QAction::triggered,
		this,
		&MainWindow::onRunSingle);

	// ── Settings ──
	auto *mSet = menuBar()->addMenu(tr("&Settings"));
	mSet->addAction(tr("My &Callsign…"), this, &MainWindow::onSetCallsign);
	mSet->addAction(
		tr("&Operator Name…"), this, &MainWindow::onSetOperName);
	mSet->addSeparator();

	// CW Speed submenu
	auto *mWpm = mSet->addMenu(tr("CW &Speed"));
	wpmGroup_ = new QActionGroup(this);
	for (int w = 10; w <= 60; w += 5) {
		auto *a = mWpm->addAction(QString("%1 WPM").arg(w));
		a->setCheckable(true);
		a->setData(w);
		wpmGroup_->addAction(a);
		connect(a, &QAction::triggered, [this, w] {
			contest_.applyWpm(w);
		});
	}

	// CW Pitch submenu
	auto *mPitch = mSet->addMenu(tr("CW &Pitch"));
	pitchGroup_ = new QActionGroup(this);
	for (int p = 300; p <= 900; p += 50) {
		auto *a = mPitch->addAction(QString("%1 Hz").arg(p));
		a->setCheckable(true);
		a->setData(p);
		pitchGroup_->addAction(a);
		connect(a, &QAction::triggered, [this, p] {
			contest_.applyPitch(p);
		});
	}

	// RX Bandwidth submenu
	auto *mBw = mSet->addMenu(tr("RX &Bandwidth"));
	bwGroup_ = new QActionGroup(this);
	for (int b : {100, 200, 300, 400, 500, 600, 700, 800, 900}) {
		auto *a = mBw->addAction(QString("%1 Hz").arg(b));
		a->setCheckable(true);
		a->setData(b);
		bwGroup_->addAction(a);
		connect(a, &QAction::triggered, [this, b] {
			contest_.applyBandwidth(b);
		});
	}

	mSet->addSeparator();
	actQsk_ = mSet->addAction(tr("&QSK"));
	actQsk_->setCheckable(true);
	connect(actQsk_, &QAction::toggled, [](bool v) {
		Ini::instance().qsk = v;
	});

	mSet->addSeparator();
	actQrn_ = mSet->addAction(tr("QR&N"));
	actQrn_->setCheckable(true);
	actQrm_ = mSet->addAction(tr("QR&M"));
	actQrm_->setCheckable(true);
	actQsb_ = mSet->addAction(tr("QS&B"));
	actQsb_->setCheckable(true);
	actFlutter_ = mSet->addAction(tr("&Flutter"));
	actFlutter_->setCheckable(true);
	actLids_ = mSet->addAction(tr("&LIDs"));
	actLids_->setCheckable(true);
	connect(actQrn_, &QAction::toggled, [](bool v) {
		Ini::instance().qrn = v;
	});
	connect(actQrm_, &QAction::toggled, [](bool v) {
		Ini::instance().qrm = v;
	});
	connect(actQsb_, &QAction::toggled, [](bool v) {
		Ini::instance().qsb = v;
	});
	connect(actFlutter_, &QAction::toggled, [](bool v) {
		Ini::instance().flutter = v;
	});
	connect(actLids_, &QAction::toggled, [](bool v) {
		Ini::instance().lids = v;
	});

	mSet->addSeparator();

	// Activity submenu
	auto *mAct = mSet->addMenu(tr("&Activity"));
	actGroup_ = new QActionGroup(this);
	for (int i = 1; i <= 9; ++i) {
		auto *a = mAct->addAction(QString::number(i));
		a->setCheckable(true);
		a->setData(i);
		actGroup_->addAction(a);
		connect(a, &QAction::triggered, [i] {
			Ini::instance().activity = i;
		});
	}

	// Duration submenu
	auto *mDur = mSet->addMenu(tr("&Duration"));
	durGroup_ = new QActionGroup(this);
	for (int d : {5, 10, 15, 30, 60, 90, 120}) {
		auto *a = mDur->addAction(QString("%1 min").arg(d));
		a->setCheckable(true);
		a->setData(d);
		durGroup_->addAction(a);
		connect(a, &QAction::triggered, [d] {
			Ini::instance().duration = d;
		});
	}

	// ── Help ──
	auto *mHelp = menuBar()->addMenu(tr("&Help"));
	mHelp->addAction(tr("&About…"), this, &MainWindow::onAbout);
}

// ── central widget ────────────────────────────────────────────────────────────

void MainWindow::setupCentralWidget()
{
	auto &ini = Ini::instance();

	auto *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setHandleWidth(2);
	setCentralWidget(splitter);

	// ══ LEFT PANEL: status + inline settings ══════════════════════════════════
	auto *leftWidget = new QWidget(splitter);
	leftWidget->setMinimumWidth(195);
	leftWidget->setMaximumWidth(255);
	auto *leftLayout = new QVBoxLayout(leftWidget);
	leftLayout->setContentsMargins(8, 8, 8, 8);
	leftLayout->setSpacing(4);
	splitter->addWidget(leftWidget);
	splitter->setStretchFactor(0, 0);

	// Status
	lblTime_ = new QLabel("00:00", leftWidget);
	{
		QFont f;
		f.setFamily("Monospace");
		f.setStyleHint(QFont::Monospace);
		f.setPointSize(22);
		f.setBold(true);
		lblTime_->setFont(f);
	}
	lblTime_->setAlignment(Qt::AlignCenter);
	lblQso_ = new QLabel("QSO:    0", leftWidget);
	lblQso_->setFont(monoFont(10));
	lblPts_ = new QLabel("Pts:    0", leftWidget);
	lblPts_->setFont(monoFont(10));
	lblMode_ = new QLabel("--", leftWidget);
	lblMode_->setFont(monoFont(10));
	lblMode_->setAlignment(Qt::AlignCenter);

	leftLayout->addWidget(lblTime_);
	leftLayout->addWidget(lblQso_);
	leftLayout->addWidget(lblPts_);
	leftLayout->addWidget(lblMode_);

	auto *sep1 = new QFrame(leftWidget);
	sep1->setFrameShape(QFrame::HLine);
	sep1->setFrameShadow(QFrame::Sunken);
	leftLayout->addWidget(sep1);

	// ── Station settings group ─────────────────────────────────────────────
	auto *grpStn = new QGroupBox(tr("Station"), leftWidget);
	auto *frmStn = new QFormLayout(grpStn);
	frmStn->setSpacing(3);
	frmStn->setContentsMargins(6, 4, 6, 4);

	sbWpm_ = new QSpinBox(grpStn);
	sbWpm_->setRange(10, 60);
	sbWpm_->setSingleStep(5);
	sbWpm_->setSuffix(" WPM");
	sbWpm_->setValue(ini.wpm);
	sbWpm_->setFocusPolicy(Qt::NoFocus);
	connect(sbWpm_,
		QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int v) {
			contest_.applyWpm(v);
			for (auto *a : wpmGroup_->actions())
				if (a->data().toInt() == v) {
					a->setChecked(true);
					break;
				}
		});

	cbPitch_ = new QComboBox(grpStn);
	cbPitch_->setFocusPolicy(Qt::NoFocus);
	for (int p = 300; p <= 900; p += 50)
		cbPitch_->addItem(QString("%1 Hz").arg(p));
	cbPitch_->setCurrentIndex((ini.pitch - 300) / 50);
	connect(cbPitch_,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int idx) {
			int p = 300 + idx * 50;
			contest_.applyPitch(p);
			for (auto *a : pitchGroup_->actions())
				if (a->data().toInt() == p) {
					a->setChecked(true);
					break;
				}
		});

	cbBw_ = new QComboBox(grpStn);
	cbBw_->setFocusPolicy(Qt::NoFocus);
	for (int b : {100, 200, 300, 400, 500, 600, 700, 800, 900})
		cbBw_->addItem(QString("%1 Hz").arg(b));
	cbBw_->setCurrentIndex(ini.bandwidth / 100 - 1);
	connect(cbBw_,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int idx) {
			int b = (idx + 1) * 100;
			contest_.applyBandwidth(b);
			for (auto *a : bwGroup_->actions())
				if (a->data().toInt() == b) {
					a->setChecked(true);
					break;
				}
		});

	sldMon_ = new QSlider(Qt::Horizontal, grpStn);
	sldMon_->setRange(-30, 20);
	sldMon_->setValue(0);
	sldMon_->setFocusPolicy(Qt::NoFocus);
	sldMon_->setToolTip(tr("Self-monitor volume (-30..+20 dB)"));
	connect(sldMon_, &QSlider::valueChanged, [this](int v) {
		contest_.setSelfMonVolume(v);
	});
	contest_.setSelfMonVolume(0);

	chkQsk_ = new QCheckBox(tr("QSK"), grpStn);
	chkQsk_->setChecked(ini.qsk);
	chkQsk_->setFocusPolicy(Qt::NoFocus);
	connect(chkQsk_, &QCheckBox::toggled, [this](bool v) {
		Ini::instance().qsk = v;
		actQsk_->setChecked(v);
	});

	frmStn->addRow(tr("Speed:"), sbWpm_);
	frmStn->addRow(tr("Pitch:"), cbPitch_);
	frmStn->addRow(tr("BW:"), cbBw_);
	frmStn->addRow(tr("Mon:"), sldMon_);
	frmStn->addRow(chkQsk_);
	leftLayout->addWidget(grpStn);

	// ── Band conditions group ──────────────────────────────────────────────
	auto *grpBand = new QGroupBox(tr("Band"), leftWidget);
	auto *vBand = new QVBoxLayout(grpBand);
	vBand->setSpacing(2);
	vBand->setContentsMargins(6, 4, 6, 4);

	auto mkChk = [&](const QString &lbl,
			     bool val,
			     QAction *act,
			     QCheckBox **member) {
		auto *c = new QCheckBox(lbl, grpBand);
		c->setChecked(val);
		c->setFocusPolicy(Qt::NoFocus);
		*member = c;
		connect(c, &QCheckBox::toggled, [act](bool v) {
			act->setChecked(v);
		});
	};
	mkChk("QRN", ini.qrn, actQrn_, &chkQrn_);
	mkChk("QRM", ini.qrm, actQrm_, &chkQrm_);
	mkChk("QSB", ini.qsb, actQsb_, &chkQsb_);
	mkChk("Flutter", ini.flutter, actFlutter_, &chkFlutter_);
	mkChk("LIDs", ini.lids, actLids_, &chkLids_);
	vBand->addWidget(chkQrn_);
	vBand->addWidget(chkQrm_);
	vBand->addWidget(chkQsb_);
	vBand->addWidget(chkFlutter_);
	vBand->addWidget(chkLids_);

	auto *frmBand = new QFormLayout;
	frmBand->setSpacing(3);
	sbActivity_ = new QSpinBox(grpBand);
	sbActivity_->setRange(1, 9);
	sbActivity_->setValue(ini.activity);
	sbActivity_->setFocusPolicy(Qt::NoFocus);
	connect(sbActivity_,
		QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int v) {
			Ini::instance().activity = v;
			for (auto *a : actGroup_->actions())
				if (a->data().toInt() == v) {
					a->setChecked(true);
					break;
				}
		});
	sbDuration_ = new QSpinBox(grpBand);
	sbDuration_->setRange(1, 120);
	sbDuration_->setValue(ini.duration);
	sbDuration_->setSuffix(tr(" min"));
	sbDuration_->setFocusPolicy(Qt::NoFocus);
	connect(sbDuration_,
		QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int v) {
			Ini::instance().duration = v;
			for (auto *a : durGroup_->actions())
				if (a->data().toInt() == v) {
					a->setChecked(true);
					break;
				}
		});
	frmBand->addRow(tr("Activity:"), sbActivity_);
	frmBand->addRow(tr("Duration:"), sbDuration_);
	vBand->addLayout(frmBand);
	leftLayout->addWidget(grpBand);

	leftLayout->addStretch();

	// ══ RIGHT PANEL ═══════════════════════════════════════════════════════════
	auto *rightWidget = new QWidget(splitter);
	auto *rightLayout = new QVBoxLayout(rightWidget);
	rightLayout->setContentsMargins(8, 8, 8, 8);
	rightLayout->setSpacing(6);
	splitter->addWidget(rightWidget);
	splitter->setStretchFactor(1, 1);

	// Entry row
	auto *entryRow = new QHBoxLayout;
	entryRow->setSpacing(6);

	edCall_ = new QLineEdit(rightWidget);
	edCall_->setPlaceholderText("Call");
	edCall_->setMaximumWidth(180);
	edCall_->setFont(monoFont(12));
	edCall_->installEventFilter(this);
	connect(edCall_, &QLineEdit::textChanged, [this](const QString &t) {
		QString up = t.toUpper();
		if (up != t) {
			QSignalBlocker b(edCall_);
			edCall_->setText(up);
			edCall_->setCursorPosition(up.size());
		}
	});

	edRst_ = new QLineEdit(rightWidget);
	edRst_->setPlaceholderText("RST");
	edRst_->setMaximumWidth(55);
	edRst_->setFont(monoFont(12));
	edRst_->setText("599");
	edRst_->installEventFilter(this);

	edNr_ = new QLineEdit(rightWidget);
	edNr_->setPlaceholderText("NR");
	edNr_->setMaximumWidth(70);
	edNr_->setFont(monoFont(12));
	edNr_->installEventFilter(this);

	entryRow->addWidget(new QLabel("Call:", rightWidget));
	entryRow->addWidget(edCall_);
	entryRow->addWidget(new QLabel("RST:", rightWidget));
	entryRow->addWidget(edRst_);
	entryRow->addWidget(new QLabel("Nr:", rightWidget));
	entryRow->addWidget(edNr_);
	entryRow->addStretch();
	rightLayout->addLayout(entryRow);

	// Function buttons
	auto *fnRow1 = new QHBoxLayout;
	fnRow1->setSpacing(4);
	auto *fnRow2 = new QHBoxLayout;
	fnRow2->setSpacing(4);
	for (int i = 0; i < 8; ++i) {
		fnBtn_[i] = new QPushButton(rightWidget);
		fnBtn_[i]->setFixedHeight(26);
		fnBtn_[i]->setFocusPolicy(Qt::NoFocus);
		fnBtn_[i]->setContextMenuPolicy(Qt::CustomContextMenu);
		fnMenu_[i] = new QMenu(fnBtn_[i]);
		fnMenu_[i]->addAction(
			tr("Edit Macro..."), [this, i] { onEditMacro(i); });
		connect(fnBtn_[i],
			&QPushButton::customContextMenuRequested,
			[this, i](const QPoint &pos) {
				fnMenu_[i]->exec(fnBtn_[i]->mapToGlobal(pos));
			});
		(i < 4 ? fnRow1 : fnRow2)->addWidget(fnBtn_[i]);
	}
	connect(fnBtn_[0], &QPushButton::clicked, this, &MainWindow::onF1);
	connect(fnBtn_[1], &QPushButton::clicked, this, &MainWindow::onF2);
	connect(fnBtn_[2], &QPushButton::clicked, this, &MainWindow::onF3);
	connect(fnBtn_[3], &QPushButton::clicked, this, &MainWindow::onF4);
	connect(fnBtn_[4], &QPushButton::clicked, this, &MainWindow::onF5);
	connect(fnBtn_[5], &QPushButton::clicked, this, &MainWindow::onF6);
	connect(fnBtn_[6], &QPushButton::clicked, this, &MainWindow::onF7);
	connect(fnBtn_[7], &QPushButton::clicked, this, &MainWindow::onF8);
	rightLayout->addLayout(fnRow1);
	rightLayout->addLayout(fnRow2);

	// RIT row
	auto *ritRow = new QHBoxLayout;
	ritRow->setSpacing(4);
	ritRow->addWidget(new QLabel("RIT:", rightWidget));
	sldRit_ = new QSlider(Qt::Horizontal, rightWidget);
	sldRit_->setRange(-1000, 1000);
	sldRit_->setSingleStep(50);
	sldRit_->setPageStep(200);
	sldRit_->setValue(ini.rit);
	sldRit_->setTickPosition(QSlider::TicksBelow);
	sldRit_->setTickInterval(200);
	sldRit_->setFocusPolicy(Qt::NoFocus);
	lblRit_ = new QLabel("  0 Hz", rightWidget);
	lblRit_->setFont(monoFont());
	lblRit_->setMinimumWidth(65);
	auto *btnRitReset = new QPushButton("0", rightWidget);
	btnRitReset->setFixedWidth(26);
	btnRitReset->setFixedHeight(22);
	btnRitReset->setFocusPolicy(Qt::NoFocus);
	btnRitReset->setToolTip(tr("Reset RIT"));
	ritRow->addWidget(sldRit_, 1);
	ritRow->addWidget(lblRit_);
	ritRow->addWidget(btnRitReset);
	rightLayout->addLayout(ritRow);

	connect(sldRit_,
		&QSlider::valueChanged,
		this,
		&MainWindow::onRitChanged);
	connect(btnRitReset,
		&QPushButton::clicked,
		this,
		&MainWindow::onRitReset);

	// Log table: Time | Call | RST | NR | TrueCall | TrueNR | Chk
	tblLog_ = new QTableWidget(0, 7, rightWidget);
	tblLog_->setHorizontalHeaderLabels(
		{"Time", "Call", "RST", "NR", "TrueCall", "TrueNR", "Chk"});
	tblLog_->horizontalHeader()->setSectionResizeMode(
		1, QHeaderView::Stretch);
	tblLog_->horizontalHeader()->setSectionResizeMode(
		4, QHeaderView::Stretch);
	tblLog_->horizontalHeader()->setDefaultSectionSize(52);
	tblLog_->verticalHeader()->setVisible(false);
	tblLog_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	tblLog_->setSelectionBehavior(QAbstractItemView::SelectRows);
	tblLog_->setAlternatingRowColors(true);
	tblLog_->setShowGrid(false);
	tblLog_->setFont(monoFont());
	tblLog_->setFocusPolicy(Qt::NoFocus);
	tblLog_->setTabKeyNavigation(false);
	rightLayout->addWidget(tblLog_, 1);

	// Tab order: Call -> NR only (RST skipped)
	setTabOrder(edCall_, edNr_);
	setTabOrder(edNr_, edCall_);

	edCall_->setFocus();
}

// ── shortcuts ─────────────────────────────────────────────────────────────────

void MainWindow::setupShortcuts()
{
	auto sc = [this](QKeySequence k, auto slot) {
		auto *s = new QShortcut(k, this);
		s->setContext(Qt::ApplicationShortcut);
		connect(s, &QShortcut::activated, this, slot);
	};
	sc(Qt::Key_F1, &MainWindow::onF1);
	sc(Qt::Key_F2, &MainWindow::onF2);
	sc(Qt::Key_F3, &MainWindow::onF3);
	sc(Qt::Key_F4, &MainWindow::onF4);
	sc(Qt::Key_F5, &MainWindow::onF5);
	sc(Qt::Key_F6, &MainWindow::onF6);
	sc(Qt::Key_F7, &MainWindow::onF7);
	sc(Qt::Key_F8, &MainWindow::onF8);
	sc(QKeySequence("Ctrl+W"), &MainWindow::onWipe);
	sc(QKeySequence("Alt+W"), &MainWindow::onWipe);
	sc(QKeySequence("Ctrl+Up"), [this] { adjustRit(+200); });
	sc(QKeySequence("Ctrl+Down"), [this] { adjustRit(-200); });
	sc(Qt::Key_PageUp, [this] { adjustWpm(+5); });
	sc(Qt::Key_PageDown, [this] { adjustWpm(-5); });
	sc(Qt::Key_Escape, [this] {
		if (!running_)
			return;
		if (contest_.myStation.msg.count(StationMessage::HisCall) > 0)
			gCallSent = false;
		if (contest_.myStation.msg.count(StationMessage::NR) > 0)
			gNrSent = false;
		contest_.abortCurrentTransmit();
	});
}

// ── event filter: Space / Enter in entry fields ───────────────────────────────

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() != QEvent::KeyPress)
		return QMainWindow::eventFilter(watched, event);
	auto *ke = static_cast<QKeyEvent *>(event);
	if (watched == edCall_ || watched == edRst_ || watched == edNr_) {
		if (ke->key() == Qt::Key_Space) {
			processSpace();
			return true;
		}
		if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
			processEnter();
			return true;
		}
		// Up/Down adjust RIT by ±50 Hz even while typing — QLineEdit would
		// otherwise consume these keys silently without propagating them.
		if (ke->key() == Qt::Key_Up &&
			!(ke->modifiers() & Qt::ControlModifier)) {
			adjustRit(+50);
			return true;
		}
		if (ke->key() == Qt::Key_Down &&
			!(ke->modifiers() & Qt::ControlModifier)) {
			adjustRit(-50);
			return true;
		}
	}
	return QMainWindow::eventFilter(watched, event);
}

void MainWindow::processSpace()
{
	QWidget *fw = focusWidget();
	if (fw == edCall_ || fw == edRst_) {
		if (edRst_->text().isEmpty())
			edRst_->setText("599");
		edNr_->setFocus();
		edNr_->selectAll();
	} else {
		edCall_->setFocus();
		edCall_->selectAll();
	}
}

void MainWindow::processEnter()
{
	if (!running_)
		return;
	if (edCall_->text().trimmed().isEmpty()) {
		onF1();
		return;
	}
	if (gCallSent && gNrSent && !edNr_->text().trimmed().isEmpty()) {
		onF3();
		return;
	}
	onF2();
}

// ── sync left panel widgets to ini ───────────────────────────────────────────

void MainWindow::syncLeftPanelToIni()
{
	auto &ini = Ini::instance();
	if (sbWpm_) {
		QSignalBlocker b(sbWpm_);
		sbWpm_->setValue(ini.wpm);
	}
	if (cbPitch_) {
		QSignalBlocker b(cbPitch_);
		cbPitch_->setCurrentIndex((ini.pitch - 300) / 50);
	}
	if (cbBw_) {
		QSignalBlocker b(cbBw_);
		cbBw_->setCurrentIndex(ini.bandwidth / 100 - 1);
	}
	if (chkQsk_) {
		QSignalBlocker b(chkQsk_);
		chkQsk_->setChecked(ini.qsk);
	}
	if (chkQrn_) {
		QSignalBlocker b(chkQrn_);
		chkQrn_->setChecked(ini.qrn);
	}
	if (chkQrm_) {
		QSignalBlocker b(chkQrm_);
		chkQrm_->setChecked(ini.qrm);
	}
	if (chkQsb_) {
		QSignalBlocker b(chkQsb_);
		chkQsb_->setChecked(ini.qsb);
	}
	if (chkFlutter_) {
		QSignalBlocker b(chkFlutter_);
		chkFlutter_->setChecked(ini.flutter);
	}
	if (chkLids_) {
		QSignalBlocker b(chkLids_);
		chkLids_->setChecked(ini.lids);
	}
	if (sbActivity_) {
		QSignalBlocker b(sbActivity_);
		sbActivity_->setValue(ini.activity);
	}
	if (sbDuration_) {
		QSignalBlocker b(sbDuration_);
		sbDuration_->setValue(ini.duration);
	}
}

// ── sync menu checkmarks to ini ───────────────────────────────────────────────

void MainWindow::syncMenuToIni()
{
	auto &ini = Ini::instance();
	actQsk_->setChecked(ini.qsk);
	actQrn_->setChecked(ini.qrn);
	actQrm_->setChecked(ini.qrm);
	actQsb_->setChecked(ini.qsb);
	actFlutter_->setChecked(ini.flutter);
	actLids_->setChecked(ini.lids);
	actRecording_->setChecked(ini.saveWav);

	for (auto *a : wpmGroup_->actions())
		if (a->data().toInt() == ini.wpm) {
			a->setChecked(true);
			break;
		}
	for (auto *a : pitchGroup_->actions())
		if (a->data().toInt() == ini.pitch) {
			a->setChecked(true);
			break;
		}
	for (auto *a : bwGroup_->actions())
		if (a->data().toInt() == ini.bandwidth) {
			a->setChecked(true);
			break;
		}
	for (auto *a : actGroup_->actions())
		if (a->data().toInt() == ini.activity) {
			a->setChecked(true);
			break;
		}
	for (auto *a : durGroup_->actions())
		if (a->data().toInt() == ini.duration) {
			a->setChecked(true);
			break;
		}

	char buf[16];
	std::snprintf(buf, sizeof(buf), "%+4d Hz", ini.rit);
	lblRit_->setText(buf);
	sldRit_->setValue(ini.rit);
}

// ── key press (RIT Up/Down) ───────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent *e)
{
	QWidget *fw = focusWidget();
	bool inEntry = (fw == edCall_ || fw == edNr_ || fw == edRst_);
	if (!inEntry) {
		if (e->key() == Qt::Key_Up &&
			!(e->modifiers() & Qt::ControlModifier)) {
			adjustRit(+50);
			return;
		}
		if (e->key() == Qt::Key_Down &&
			!(e->modifiers() & Qt::ControlModifier)) {
			adjustRit(-50);
			return;
		}
	}
	QMainWindow::keyPressEvent(e);
}

// ── run slots ─────────────────────────────────────────────────────────────────

void MainWindow::onRunPileup()
{
	contest_.stop();
	audio_.stop();
	contest_.setRunMode(RunMode::Pileup);
	lblMode_->setText("PILE-UP");
	tblLog_->setRowCount(0);
	nextNr_ = 1;
	contest_.start();
	auto &ini = Ini::instance();
	audio_.start(DEFAULTRATE, ini.bufSize, [this] {
		return contest_.getAudioBlock();
	});
	running_ = true;
}

void MainWindow::onRunSingle()
{
	contest_.stop();
	audio_.stop();
	contest_.setRunMode(RunMode::Single);
	lblMode_->setText("SINGLE");
	tblLog_->setRowCount(0);
	nextNr_ = 1;
	contest_.start();
	auto &ini = Ini::instance();
	audio_.start(DEFAULTRATE, ini.bufSize, [this] {
		return contest_.getAudioBlock();
	});
	running_ = true;
}

void MainWindow::onRunStop()
{
	audio_.stop();
	contest_.stop();
	lblMode_->setText("──");
	actRunPileup_->setChecked(false);
	actRunSingle_->setChecked(false);
	running_ = false;
}

// ── function key slots ────────────────────────────────────────────────────────

void MainWindow::onF1()
{
	sendMacro(0);
}
void MainWindow::onF2()
{
	sendMacro(1);
}
void MainWindow::onF3()
{
	sendMacro(2);
}
void MainWindow::onF4()
{
	sendMacro(3);
}
void MainWindow::onF5()
{
	sendMacro(4);
}
void MainWindow::onF6()
{
	sendMacro(5);
}
void MainWindow::onF7()
{
	sendMacro(6);
}
void MainWindow::onF8()
{
	sendMacro(7);
}
void MainWindow::onWipe()
{
	wipeEntry();
}

// ── RIT ───────────────────────────────────────────────────────────────────────

void MainWindow::onRitChanged(int hz)
{
	// snap to nearest 50
	int snapped = (hz / 50) * 50;
	contest_.setRit(snapped);
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%+4d Hz", snapped);
	lblRit_->setText(buf);
}

void MainWindow::onRitReset()
{
	sldRit_->setValue(0);
	contest_.setRit(0);
	lblRit_->setText("   0 Hz");
}

void MainWindow::adjustRit(int delta)
{
	int v = std::clamp(sldRit_->value() + delta, -1000, 1000);
	sldRit_->setValue(v);
}

void MainWindow::adjustWpm(int delta)
{
	auto &ini = Ini::instance();
	int w = std::clamp(ini.wpm + delta, 10, 60);
	contest_.applyWpm(w);
	// sync menu checkmark
	for (auto *a : wpmGroup_->actions())
		if (a->data().toInt() == w) {
			a->setChecked(true);
			break;
		}
}

// ── timer / score callbacks ───────────────────────────────────────────────────

void MainWindow::onTimerTick(double elapsed)
{
	int m = static_cast<int>(elapsed) / 60;
	int s = static_cast<int>(elapsed) % 60;
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
	lblTime_->setText(buf);
}

void MainWindow::onScoreChanged(int qso, int pts)
{
	lblQso_->setText(QString("QSO: %1").arg(qso));
	lblPts_->setText(QString("Pts: %1").arg(pts));
}

void MainWindow::onCallDecoded(const QString &call)
{
	edCall_->setText(call);
}

// ── settings dialogs ──────────────────────────────────────────────────────────

void MainWindow::onSetCallsign()
{
	bool ok;
	QString s = QInputDialog::getText(this,
		tr("My Callsign"),
		tr("Callsign:"),
		QLineEdit::Normal,
		QString::fromStdString(Ini::instance().call),
		&ok);
	if (ok && !s.isEmpty())
		Ini::instance().call = s.toUpper().toStdString();
}

void MainWindow::onSetOperName()
{
	bool ok;
	QString s = QInputDialog::getText(this,
		tr("Operator Name"),
		tr("Name:"),
		QLineEdit::Normal,
		QString::fromStdString(Ini::instance().hamName),
		&ok);
	if (ok)
		Ini::instance().hamName = s.toStdString();
}

void MainWindow::onAbout()
{
	QMessageBox::about(this,
		tr("About CMorseRunner"),
		tr("<b>CMorseRunner</b><br>"
		   "A CW contest trainer.<br><br>"
		   "Based on the original MorseRunner by VE3NEA.<br>"
		   "Refractored with C++ and Qt6 by Izumi Chino@BI6DX Team<br>"
		   "This repository may be distributed under GPL-2.0 and "
		   "MPL-2.0"));
}

// ── helpers ───────────────────────────────────────────────────────────────────

void MainWindow::wipeEntry()
{
	edCall_->clear();
	edNr_->clear();
	edRst_->setText("599");
	edCall_->setFocus();
}

void MainWindow::sendExchange()
{
	contest_.userSentExchange(enteredCall(), nextNr_);
}

std::string MainWindow::macroText(int index) const
{
	return Ini::instance().macros[static_cast<size_t>(index)];
}

std::string MainWindow::buildExchangeText() const
{
	return "5NN " + std::to_string(nextNr_);
}

std::string MainWindow::buildTuReplyText() const
{
	static constexpr const char *variants[] = {
		"TU TEST $MYCALL",
		"TU $MYCALL TEST",
		"TU TEST",
		"TU",
	};

	return expandMacro(variants[std::rand() % 4]);
}

std::string MainWindow::enteredCall() const
{
	return edCall_->text().trimmed().toStdString();
}

int MainWindow::enteredRst() const
{
	bool ok = false;
	int rst = edRst_->text().toInt(&ok);

	return ok ? rst : 599;
}

int MainWindow::enteredNr() const
{
	bool ok = false;
	int nr = edNr_->text().toInt(&ok);

	return ok ? nr : 0;
}

std::string MainWindow::expandMacro(const std::string &macro) const
{
	std::string result = macro;
	auto replaceAll = [&](const std::string &token,
				  const std::string &value) {
		size_t pos = 0;
		while ((pos = result.find(token, pos)) != std::string::npos) {
			result.replace(pos, token.size(), value);
			pos += value.size();
		}
	};

	replaceAll("$MYCALL", Ini::instance().call);
	replaceAll("$HIS", enteredCall());
	replaceAll("$EXCH", buildExchangeText());
	return result;
}

void MainWindow::updateMacroLabels()
{
	for (int i = 0; i < 8; ++i)
		if (fnBtn_[i])
			fnBtn_[i]->setText(macroButtonLabel(i, macroText(i)));
}

void MainWindow::onEditMacro(int index)
{
	bool ok = false;
	QString text = QInputDialog::getText(this,
		tr("Edit Macro F%1").arg(index + 1),
		tr("Macro text ($MYCALL, $HIS, $EXCH):"),
		QLineEdit::Normal,
		QString::fromStdString(macroText(index)),
		&ok);
	if (!ok)
		return;
	Ini::instance().macros[static_cast<size_t>(index)] = text.toStdString();
	updateMacroLabels();
}

void MainWindow::sendMacro(int index)
{
	if (!running_)
		return;
	const std::string text = expandMacro(macroText(index));
	if (text.empty())
		return;
	switch (index) {
	case 0:
		contest_.userSentCqText(text);
		wipeEntry();
		break;

	case 1: {
		contest_.userSentExchangeText(text, enteredCall(), nextNr_);
		break;
	}

	case 2: {
		std::string tuText = text;

		if (macroText(2) == "TU")
			tuText = buildTuReplyText();

		contest_.userSentTuAndLogText(
			tuText, "", enteredCall(), enteredRst(), enteredNr());
		wipeEntry();
		++nextNr_;
		break;
	}

	case 3:
		contest_.userSentText(text, MsgSet{StationMessage::Agn});
		break;

	case 4:
		contest_.userSentText(text, MsgSet{StationMessage::Qm});
		break;

	case 5:
		contest_.userSentText(text, MsgSet{StationMessage::B4});
		break;

	case 6:
		contest_.userSentText(text, MsgSet{StationMessage::HisCall});
		break;

	case 7:
		contest_.userSentText(text, MsgSet{StationMessage::Nil});
		break;

	default:
		break;
	}
}

void MainWindow::applyQsoRow(const Qso &q)
{
	int row = tblLog_->rowCount();
	tblLog_->insertRow(row);

	int totalMin = static_cast<int>(q.t * 1440);
	char tbuf[16];
	std::snprintf(
		tbuf, sizeof(tbuf), "%02d:%02d", totalMin / 60, totalMin % 60);

	auto cell = [](const QString &s) { return new QTableWidgetItem(s); };
	tblLog_->setItem(row, 0, cell(tbuf));
	tblLog_->setItem(row, 1, cell(QString::fromStdString(q.call)));
	tblLog_->setItem(row, 2, cell(QString::number(q.rst)));
	tblLog_->setItem(row, 3, cell(QString::number(q.nr)));
	tblLog_->setItem(row, 4, cell(QString::fromStdString(q.trueCall)));
	tblLog_->setItem(row,
		5,
		cell(q.trueNr > 0 ? QString::number(q.trueNr) : QString()));
	tblLog_->setItem(row, 6, cell(QString::fromStdString(q.err).trimmed()));

	QColor bg;
	if (q.err == "NIL")
		bg = QColor(220, 50, 50, 110);
	else if (q.err == "DUP")
		bg = QColor(220, 140, 30, 110);
	else if (q.err == "RST" || q.err == "NR ")
		bg = QColor(210, 210, 30, 110);
	else
		bg = QColor(50, 200, 50, 70);

	for (int c = 0; c < 7; ++c)
		if (tblLog_->item(row, c))
			tblLog_->item(row, c)->setBackground(bg);

	tblLog_->scrollToBottom();
}
