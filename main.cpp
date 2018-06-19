#include <QCoreApplication>

#include "ftp2git.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Ftp2Git ftp2Git;

    return a.exec();
}
