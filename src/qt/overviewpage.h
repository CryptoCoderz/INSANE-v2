#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>

// gauge depends
#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"

// gauge depends
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

// gauge defines
double getPoSHardness(int);
double convertPoSCoins(int64_t);
int getPoSTime(int);
int PoSInPastHours(int);
const CBlockIndex* getPoSIndex(int);

namespace Ui {
    class OverviewPage;
}
class WalletModel;
class ClientModel;
class TxViewDelegate;
class TransactionFilterProxy;

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

    //gauge defines
    void blockCalled();
    void updatePoSstat(bool);

signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    WalletModel *model;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
    ClientModel *clientModel;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
};

#endif // OVERVIEWPAGE_H
