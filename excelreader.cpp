#include "excelreader.h"
#include <QDebug>

int excelReader::keyLength = 17;
QString excelReader::keyName = "账号";
QString excelReader::valueName = "贷款余额";
QString excelReader::dateName = "首次放款日";
QString excelReader::dataPrefix = "PL";
int excelReader::newMonthBound = 21;

QString excelReader::pointToString(int x, int y)
{
    int A = 'A';
    return QChar(A + x -1) + QString::number(y);
}

excelReader::excelReader(QString& filePath)
{
    if(!filePath.isEmpty()){
        excel = new QAxObject("Excel.Application");
        excel->setProperty("Visible", false);
        excel->setProperty("DisplayAlerts", false);

        workbooks = excel->querySubObject("WorkBooks");
        workbooks->dynamicCall("Open (const QString&)", filePath);
        workbook = excel->querySubObject("ActiveWorkBook");
        //prepare
        worksheet = workbook->querySubObject("Worksheets(int)", 1);
        usedRange = worksheet->querySubObject("UsedRange");
        rowStart = usedRange->property("Row").toInt();
        colStart = usedRange->property("Column").toInt();
        rowEnd = usedRange->querySubObject("Rows")->property("Count").toInt() + rowStart - 1;
        colEnd = usedRange->querySubObject("Columns")->property("Count").toInt() + colStart - 1;
    }
}

void excelReader::getIndex()
{
    QSharedPointer<QAxObject> cell;
    for (int i = rowStart; i <= rowEnd; i++) {
        for (int j = colStart; j <= colEnd; j++) {
            cell = (QSharedPointer<QAxObject>) worksheet->querySubObject("Cells(Int, Int)", i, j);
            if(cell->property("Value").toString() == keyName) {
                keyIndex = j;
                dataStart = i + 1;
            } else{
                if(cell->property("Value").toString() == dateName) {
                    dateIndex = j;
                } else{
                    if(cell->property("Value").toString() == valueName) {
                        valueIndex = j;
                        return;
                    }
                }
            }
        }
    }
}

void excelReader::getKey(){
    keyList = (QSharedPointer<QStringList>) new QStringList();
    QSharedPointer<QAxObject> range = (QSharedPointer<QAxObject>) worksheet->querySubObject( "Range(const QVariant&)", QVariant(pointToString(keyIndex, dataStart) + ":" + pointToString(keyIndex, rowEnd)));
    QSharedPointer<QVariantList> all = (QSharedPointer<QVariantList>) new QVariantList(range->property("Value").toList());
    int count = all->count();
    QString key;
    for(int i = 0; i < count; i++) {
        key = all->at(i).toList().at(0).toString();
        if(key.isEmpty()){
            dataEnd = dataStart + i - 1;
            return;
        } else{
            keyList->append(key.rightJustified(keyLength, '0'));
        }
    }
}

void excelReader::checkNewMonth()
{
    //check strategy:
    //change dataEnd to the last row need to be update
    QSharedPointer<QAxObject> cell;
    int day;
    for (int i = dataEnd; i >= dataStart; i--) {
        //remember in excel, Cells(Int, Int) in the format (row, col)
        cell = (QSharedPointer<QAxObject>) worksheet->querySubObject("Cells(Int, Int)", i, dateIndex);
        day = cell->property("Value").toString().right(2).toInt();
        if(day < newMonthBound) {
            dataEnd = i;
            return;
        } else{
            keyList->removeAt(i-dataStart);
        }
    }
}

void excelReader::getValue()
{
    valueList = (QSharedPointer<QStringList>) new QStringList();
    int count = keyList->count();
    QString value;
    for(int i = 0; i < count; i++) {
        value = keyValueMap->value(keyList->at(i));
        if(value.isEmpty()) {
            value = "NULL";
        }
        valueList->append(value);
    }
}

void excelReader::pushValue()
{
    QSharedPointer<QAxObject> range = (QSharedPointer<QAxObject>) worksheet->querySubObject( "Range(const QVariant&)", QVariant(pointToString(valueIndex, dataStart) + ":" + pointToString(valueIndex, dataEnd)));
    range->dynamicCall("SetValue(const QVariant&)", *pack(valueList));
}

QSharedPointer<QVariant> excelReader::pack(QSharedPointer<QStringList> list)
{
    int count = list->count();
    QSharedPointer<QVariantList> container = (QSharedPointer<QVariantList>) new QVariantList();
    QVariantList wrapper;
    for(int i = 0; i < count; i++) {
        wrapper.clear();
        wrapper.append(valueList->at(i));
        container->append(QVariant(wrapper));
    }
    QSharedPointer<QVariant> package = (QSharedPointer<QVariant>) new QVariant(*container);
    return package;
}

void excelReader::exec(QSharedPointer<QMap<QString, QString> > map)
{
    keyValueMap = map;
    this->getIndex();
    this->getKey();
    this->checkNewMonth();
    this->getValue();
    this->pushValue();
}

excelReader::~excelReader()
{
    workbook->dynamicCall("Save(void)");
    workbook->dynamicCall("Close (Boolean)", false);

    delete excel;

    /* seems delete these will crash the program
     * may due to COM
    delete workbooks;
    delete workbook;
    delete worksheet;
    delete usedRange;*/
}
