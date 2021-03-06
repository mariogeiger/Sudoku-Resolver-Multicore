#include "widget.h"
#include "ui_widget.h"
#include <qglobal.h>
#include <QInputDialog>

Widget::Widget(QWidget *parent)
    : QWidget(parent), ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->label->setText(trUtf8("SudoKu Resolver detected a %1 core processor on your computer.").arg(threadHelper.cpuThreads()));//
    ui->verticalLayout_2->addWidget(&plateau);

    showTimer = new QTimer(this);
    connect(showTimer, SIGNAL(timeout()), this, SLOT(afficherNbr()));
    bmTimer = new QTimer(this);
    connect(bmTimer, SIGNAL(timeout()), this, SLOT(verifBenchmark()));
    _progress = new QProgressDialog(this);
    _progress->setCancelButtonText(trUtf8("&Stop"));
    _progress->setWindowTitle(trUtf8("Search solutions"));
    _progress->setRange(0, 0);
    _benchmarkMode = 0;

    connect(_progress, SIGNAL(canceled()), &threadHelper, SLOT(stop()));
    connect(&threadHelper, SIGNAL(finished()), this, SLOT(stopEvent()));

    plateau.focusCentralCase();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_search_clicked()
{
    plateau.setAllDisplayMode(SudokuCaseWidget::Edit);
    SudokuGame jeu = plateau.getGrille().toSudokuGame();

    if(ui->comboBox->currentIndex() != 0)
        jeu = SudokuGame();

    if(!jeu.verification())
    {
        QMessageBox::information(this, trUtf8("Error"), trUtf8("There is an error in the grid"));
    }
    else
    {
        setEnabled(false);

        _progress->setLabelText(trUtf8("Emptying memory..."));
        _progress->setEnabled(true);
        _progress->show();

        qApp->processEvents();

		showTimer->setInterval(900);
		showTimer->start();
		if (_benchmarkMode) {
			bmTimer->setInterval(50);
			bmTimer->start();
		}

		threadHelper.start(jeu);

        dernierTemps = 0.0;
        dernierDecompte = 0l;
        chrono.start();
        _progress->setLabelText(trUtf8("Serching...\nWith %1 thread(s).").arg(threadHelper.nthr()));
    }
}

void Widget::verifBenchmark()
{
    if(_benchmarkMode && threadHelper.nbrall() >= _benchmarkMode) {
        _progress->cancel();
        threadHelper.stop();
    }
}

#define SECONDES_DE(temps)  ((double)((int)(temps * 100.0) % (60 * 100)) / 100.0)
#define MINUTES_DE(temps)   ((int)temps / 60)

void Widget::afficherNbr()
{
    _temps = chrono.elapsed() / 1000.0;

    QString saveStatu(trUtf8("Saving solutions..."));

    if(threadHelper.nbrall() > threadHelper.nbrsave())
    {
        saveStatu = trUtf8("Saving halted ! (%1)").arg(toStrKMG(threadHelper.nbrsave()));
        if(!_benchmarkMode) plateau.setGrille(SudokuData());
    }
    else
    {
        if(!_benchmarkMode) plateau.setGrille(threadHelper.solAt(threadHelper.nbrsave()-1));
    }

    double scoreActuel = (double)(threadHelper.nbrall() - dernierDecompte) / (_temps - dernierTemps);
    dernierTemps = _temps;
    dernierDecompte = threadHelper.nbrall();

    _progress->setLabelText(trUtf8("%1 solutions found... ( %L2 /s )\nIn %3 m %4 s, with %5 thread(s).\n%6")
                            .arg(toStrKMG(threadHelper.nbrall())).arg(scoreActuel)
                            .arg(MINUTES_DE(_temps)).arg(SECONDES_DE(_temps)).arg(threadHelper.nthr()).arg(saveStatu));
}

void Widget::stopEvent()
{
    _progress->cancel();

    showTimer->stop();
    bmTimer->stop();

    _temps = chrono.elapsed() / 1000.0;

    if(threadHelper.nbrall())
        ui->spinBox->setMaximum(threadHelper.nbrall());
    else
        ui->spinBox->setMaximum(1);

    ui->spinBox->setValue(1);

	if(_benchmarkMode == 0 && threadHelper.nbrall() != 0)
        on_spinBox_valueChanged(1);

	if(_benchmarkMode == 0 && threadHelper.nbrall() == 0)
        QMessageBox::information(this, trUtf8("Finished"), trUtf8("No solution found"));
    else if(!_benchmarkMode && _temps == 0)
        QMessageBox::information(this, trUtf8("Finished"), trUtf8("%L1 solution(s) found")
                                 .arg(threadHelper.nbrall()));
    else if(!_benchmarkMode)
        QMessageBox::information(this, trUtf8("Finished"), trUtf8("%L1 solution(s) found\nin %2 m %3 s ( %L4 /s )")
                                 .arg(threadHelper.nbrall()).arg(MINUTES_DE(_temps)).arg(SECONDES_DE(_temps)).arg((double)threadHelper.nbrall()/_temps));
    else if(_benchmarkMode && threadHelper.nbrall() >= _benchmarkMode)
    {
        QString score = trUtf8(" Mode : %1\n Score : %L2 solutions/s\n Time : %3 m %4 s").arg(ui->comboBox->currentText()).arg((double)threadHelper.nbrall()/_temps).arg(MINUTES_DE(_temps)).arg(SECONDES_DE(_temps));
        if(QMessageBox::question(this, trUtf8("Benchmark finished"), trUtf8("Do you want to send your score ?\n%1").arg(score),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes)
        {
            sauvegarderLeScore();
        }
    }

    setEnabled(true);
    plateau.focusCentralCase();

    for(int i(0); i<threadHelper.tlist().size(); ++i)
        cout << QString(trUtf8("Thread #%1 : %2 solutions.")).arg(i).arg(threadHelper.tlist().at(i)->nbrSolutions()) << endl;
}

void Widget::sauvegarderLeScore()
{
    const int ver = 210;
    bool ok;
    QString pseudo = QInputDialog::getText(this, trUtf8("Nickname"), trUtf8("Enter an nickname (it will be show with your score)"), QLineEdit::Normal, QString(), &ok);
    if (!ok) return;
    QString comment = QInputDialog::getText(this, trUtf8("Comment"), trUtf8("You can add a comment here"));
    int score = (double)threadHelper.nbrall() / _temps;
    // meta info
    QString os = trUtf8("autre");
#if defined(Q_OS_WIN32)
    os = "win32";
#elif defined(Q_OS_WIN64)
    os = "win64";
#elif defined(Q_OS_LINUX)
    os = "linux";
#elif defined(Q_OS_MAC32)
    os = "mac32";
#elif defined(Q_OS_MAC64)
    os = "mac64";
#elif defined(Q_OS_OS2)
    os = "os/2";
#endif
    int core = threadHelper.cpuThreads();

    networkManager.get(QNetworkRequest(QString("http://setup.weeb.ch/srmbm/post.php?ver=%1&pseudo=%2&comment=%3&score=%4&os=%5&core=%6")
                                       .arg(ver).arg(pseudo).arg(comment).arg(score).arg(os).arg(core)));
    QMessageBox::information(this, trUtf8("Website"), trUtf8("Go on <a href=\"http://setup.weeb.ch/srmbm/index.php\">http://setup.weeb.ch/srmbm/index.php</a>"));
}

void Widget::on_spinBox_valueChanged(int index)
{
    index--;
    SudokuData data = threadHelper.solAt(index);
    if(data[0] == 0)
        QMessageBox::information(this, trUtf8("Out of memory"), trUtf8("Data not found, the backup have been stopped at %1 input.").arg(toStrKMG(threadHelper.nbrsave())));
    else
        plateau.setGrille(data);
}

void Widget::on_pushButton_save_clicked()
{
    _sauvegarde = plateau.getGrille();
    ui->pushButton_restore->setEnabled(true);
}

void Widget::on_pushButton_restore_clicked()
{
    plateau.setGrille(_sauvegarde);
}

void Widget::on_pushButton_reset_clicked()
{
    plateau.setGrille(SudokuData());
}

void Widget::on_comboBox_currentIndexChanged(int index)
{
    static SudokuData sauvegarde;
    static bool was0 = true;
    switch(index) {
    case 0:
        _benchmarkMode = 0;
        break;
    case 1:
        _benchmarkMode = 500000;
        break;
    case 2:
        _benchmarkMode = 1000000;
        break;
    case 3:
        _benchmarkMode = 5000000;
        break;
    case 4:
        _benchmarkMode = 10000000;
        break;
    }
    bool enabled;
    if(index == 0)
    {
        was0 = true;
        plateau.setGrille(sauvegarde);
        enabled = true;
        ui->pushButton_search->setText(trUtf8("Search"));
    }
    else
    {
        if(was0) {
            sauvegarde = plateau.getGrille();
            plateau.setGrille(SudokuData());
            ui->pushButton_search->setText(trUtf8("Launch benchmark !"));
        }
        was0 = false;
        enabled = false;
    }
    plateau.setEnabled(enabled);
    ui->spinBox->setEnabled(enabled);
    ui->pushButton_restore->setEnabled(enabled);
    ui->pushButton_save->setEnabled(enabled);
    ui->pushButton_reset->setEnabled(enabled);
}

void Widget::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        on_pushButton_search_clicked();
        break;
    default:
        event->ignore();
    }
}

QString toStrKMG(long unsigned int x)
{
    int r;
    if(x/1000)
    {
        r= x%1000;
        x/=1000;
        if(x/1000)
        {
            r= x%1000;
            x/=1000;
            if(x/1000)
            {
                r= x%1000;
                x/=1000;
                if(x/1000)
                {
                    r= x%1000;
                    x/=1000;
                    // ...
                    return QString("%L1.%2T").arg(x).arg(r, 3, 10, QChar('0'));
                }
                return QString("%L1.%2G").arg(x).arg(r, 3, 10, QChar('0'));
            }
            return QString("%L1.%2M").arg(x).arg(r, 3, 10, QChar('0'));
        }
        return QString("%L1.%2k").arg(x).arg(r, 3, 10, QChar('0'));
    }
    return QString("%L1").arg(x);
}

