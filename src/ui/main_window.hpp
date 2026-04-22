#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QSlider>
#include <QAction>
#include <QActionGroup>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QMenu>
#include "../core/contest.hpp"
#include "../audio/audio_output.hpp"

class MainWindow : public QMainWindow
{
	Q_OBJECT
      public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

      protected:
	void keyPressEvent(QKeyEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

      private slots:
	void onRunPileup();
	void onRunSingle();
	void onRunStop();
	void onF1();
	void onF2();
	void onF3();
	void onF4();
	void onF5();
	void onF6();
	void onF7();
	void onF8();
	void onWipe();
	void onRitChanged(int hz);
	void onRitReset();
	void onTimerTick(double elapsed);
	void onScoreChanged(int qso, int pts);
	void onCallDecoded(const QString &call);
	void onSetCallsign();
	void onSetOperName();
	void onAbout();
	void onEditMacro(int index);

      private:
	void setupMenuBar();
	void setupCentralWidget();
	void setupShortcuts();
	void syncMenuToIni();
	void syncLeftPanelToIni();
	void applyQsoRow(const Qso &q);
	void wipeEntry();
	void sendExchange();
	void sendMacro(int index);
	std::string macroText(int index) const;
	std::string expandMacro(const std::string &macro) const;
	std::string buildExchangeText() const;
	std::string buildTuReplyText() const;
	std::string enteredCall() const;
	int enteredRst() const;
	int enteredNr() const;
	void updateMacroLabels();
	void adjustRit(int delta);
	void adjustWpm(int delta);
	void processSpace(); // Space-key field cycling: Call→NR→Call
	void processEnter(); // ESM-style Enter

	// ── Entry area ────────────────────────────────────────────────────────────
	QLineEdit *edCall_{nullptr};
	QLineEdit *edNr_{nullptr};
	QLineEdit *edRst_{nullptr};

	// ── Function buttons ──────────────────────────────────────────────────────
	QPushButton *fnBtn_[8]{};
	QMenu *fnMenu_[8]{};

	// ── RIT ───────────────────────────────────────────────────────────────────
	QSlider *sldRit_{nullptr};
	QLabel *lblRit_{nullptr};

	// ── Status labels (left panel) ────────────────────────────────────────────
	QLabel *lblTime_{nullptr};
	QLabel *lblQso_{nullptr};
	QLabel *lblPts_{nullptr};
	QLabel *lblMode_{nullptr};

	// ── Left panel inline settings ────────────────────────────────────────────
	QSpinBox *sbWpm_{nullptr};
	QComboBox *cbPitch_{nullptr};
	QComboBox *cbBw_{nullptr};
	QSlider *sldMon_{nullptr}; // self-monitor volume
	QCheckBox *chkQsk_{nullptr};
	QCheckBox *chkQrn_{nullptr};
	QCheckBox *chkQrm_{nullptr};
	QCheckBox *chkQsb_{nullptr};
	QCheckBox *chkFlutter_{nullptr};
	QCheckBox *chkLids_{nullptr};
	QSpinBox *sbActivity_{nullptr};
	QSpinBox *sbDuration_{nullptr};

	// ── Log table ─────────────────────────────────────────────────────────────
	QTableWidget *tblLog_{nullptr};

	// ── Menu actions (kept for sync) ──────────────────────────────────────────
	QAction *actRecording_{nullptr};
	QAction *actRunPileup_{nullptr};
	QAction *actRunSingle_{nullptr};
	QAction *actRunStop_{nullptr};
	QActionGroup *runGroup_{nullptr};
	QActionGroup *wpmGroup_{nullptr};
	QActionGroup *pitchGroup_{nullptr};
	QActionGroup *bwGroup_{nullptr};
	QActionGroup *actGroup_{nullptr};
	QActionGroup *durGroup_{nullptr};
	QAction *actQsk_{nullptr};
	QAction *actQrn_{nullptr};
	QAction *actQrm_{nullptr};
	QAction *actQsb_{nullptr};
	QAction *actFlutter_{nullptr};
	QAction *actLids_{nullptr};

	Contest contest_;
	AudioOutput audio_;
	bool running_{false};
	int nextNr_{1};
};
