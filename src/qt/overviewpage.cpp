#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "transactionrecord.h"
#include "init.h"
#include "bitcoinrpc.h"
#include "kernel.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QTimer>

using namespace json_spirit;

#include <sstream>
#include <string>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
//        if(qVariantCanConvert<QColor>(value))
//        {
//            foreground = qvariant_cast<QColor>(value);
//        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    // start guage timer
    QTimer::singleShot(20000,this,SLOT(blockCalled()));
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

// PoS Gauge Section below
double GetPoSKernelPS();

std::string getPoSHash(int Height)
{
    if(Height < 0) { return "351c6703813172725c6d660aa539ee6a3d7a9fe784c87fae7f36582e3b797058"; }
    int desiredheight;
    desiredheight = Height;
    if (desiredheight < 0 || desiredheight > nBestHeight)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > desiredheight)
        pblockindex = pblockindex->pprev;
    return pblockindex->phashBlock->GetHex();
}


double getPoSHardness(int height)
{
    const CBlockIndex* blockindex = getPoSIndex(height);

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;

}

const CBlockIndex* getPoSIndex(int height)
{
    std::string hex = getPoSHash(height);
    uint256 hash(hex);
    return mapBlockIndex[hash];
}

int getPoSTime(int Height)
{
    std::string strHash = getPoSHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nTime;
}

int PoSInPastHours(int hours)
{
    int wayback = hours * 3600;
    bool check = true;
    int height = pindexBest->nHeight;
    int heightHour = pindexBest->nHeight;
    int utime = (int)time(NULL);
    int target = utime - wayback;

    while(check)
    {
        if(getPoSTime(heightHour) < target)
        {
            check = false;
            return height - heightHour;
        } else {
            heightHour = heightHour - 1;
        }
    }

    return 0;
}

double convertPoSCoins(int64_t amount)
{
    return (double)amount / (double)COIN;
}

void OverviewPage::updatePoSstat(bool stat)
{
    if(stat)
    {
        uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
        pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

        uint64_t nNetworkWeight = GetPoSKernelPS();
        bool staking = nLastCoinStakeSearchInterval && nWeight;
        int nExpectedTime = staking ? (nTargetSpacing * nNetworkWeight / nWeight) : 0;

        QString Qseconds = " Second(s)";
        if(nExpectedTime > 86399)
        {
           nExpectedTime = nExpectedTime / 60 / 60 / 24;
           Qseconds = " Day(s)";
        }
        else if(nExpectedTime > 3599)
        {
           nExpectedTime = nExpectedTime / 60 / 60;
           Qseconds = " Hour(s)";
        }
        else if(nExpectedTime > 59)
        {
           nExpectedTime = nExpectedTime / 60;
           Qseconds = " Minute(s)";
        }
        ui->lbTime->show();
        ui->diffdsp->show();
        ui->hashrt->show();
        int height = pindexBest->nHeight;
        uint64_t Pawrate = GetPoSKernelPS();
        double Pawrate2 = ((double)Pawrate / 1);
        QString QPawrate = QString::number(Pawrate2, 'f', 2);
        ui->hashrt->setText(QPawrate + " INSANE");
        if(Pawrate2 > 999999999)
        {
           Pawrate2 = (Pawrate2 / 1000000000);
           QPawrate = QString::number(Pawrate2, 'f', 2);
           ui->hashrt->setText(QPawrate + " BILLION INSANE");
        }
        else if(Pawrate2 > 999999)
        {
           Pawrate2 = (Pawrate2 / 1000000);
           QPawrate = QString::number(Pawrate2, 'f', 2);
           ui->hashrt->setText(QPawrate + " MILLION INSANE");
        }
        else if(Pawrate2 > 999)
        {
           Pawrate2 = (Pawrate2 / 1000);
           QPawrate = QString::number(Pawrate2, 'f', 2);
           ui->hashrt->setText(QPawrate + " THOUSAND INSANE");
        }
        double hardness = getPoSHardness(height);
        uint64_t nStakePercentage = (double)nWeight / (double)nNetworkWeight * 100;
        uint64_t nNetPercentage = (100 - (double)nStakePercentage);
        if(nWeight > nNetworkWeight)
        {
            nStakePercentage = (double)nNetworkWeight / (double)nWeight * 100;
            nNetPercentage = (100 - (double)nStakePercentage);
        }
        CBlockIndex* pindex = pindexBest;
        QString QHardness = QString::number(hardness, 'f', 6);
        QString QStakePercentage = QString::number(nStakePercentage, 'f', 2);
        QString QNetPercentage = QString::number(nNetPercentage, 'f', 2);
        QString QTime = clientModel->getLastBlockDate().toString();
        QString QExpect = QString::number(nExpectedTime, 'f', 0);
        QString QStaking = "DISABLED";
        QString QStakeEN = "NOT STAKING";
        ui->estnxt->setText(QExpect + Qseconds);
        ui->stkstat->setText(QStakeEN);
        if(!pindex->IsProofOfStake())
        {
            QHardness = "Block is PoW";
            QPawrate = "Block is PoW";
            ui->hashrt->setText(QPawrate);
        }
        if(nExpectedTime == 0)
        {
            QExpect = "NOT STAKING";
            ui->estnxt->setText(QExpect);
        }
        if(staking)
        {
            QStakeEN = "STAKING";
            ui->stkstat->setText(QStakeEN);
        }
        if(GetBoolArg("-staking", true))
        {
            QStaking = "ENABLED";
        }
        ui->lbTime->setText(QTime);
        ui->diffdsp->setText(QHardness);
        ui->stken->setText(QStaking);
        ui->urwheight->setValue(QStakePercentage.toDouble());
        ui->netweight->setValue(QNetPercentage.toDouble());
        if(nStakePercentage < 1)
        {
            ui->urwheight->setValue(1);
            ui->netweight->setValue(99);
        }
        else if(nStakePercentage > 99)
        {
            ui->netweight->setValue(1);
            ui->urwheight->setValue(99);
        }
// TODO: DISPLAY STAKING STATISTICS
        ui->hourlydsp->setText("0");
        ui->dailydsp->setText("0");
        ui->weeklydsp->setText("0");
        ui->monthlydsp->setText("0");

        QTimer::singleShot(10000,this,SLOT(blockCalled()));
    }
}

void OverviewPage::blockCalled()
{
    updatePoSstat(true);
}

