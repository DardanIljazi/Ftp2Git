#ifndef FTPMANAGER_H
#define FTPMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>

#include "qftp/qftp.h"

// FTP
#define HOST        ""
#define FTP_FOLDER  ""
#define USERNAME    ""
#define PASSWORD    ""
#define PORT        "21" // Only support port 21 (sftp is not supported by QFtp)

#define TREEOUTPUT_FILENAME "treeOutput.txt" // The tree of the Ftp will be outputed on this file

// PHPMYADMIN
#define PHPMYADMIN_URL      "" // For example https://yourwebsite.com/phpmyadmin/ (the connection page of phpyadmin, WARNING: YOU MUST PUT THE / at the end of the link)
#define PHPMYADMIN_USER     ""
#define PHPMYADMIN_PASSWORD ""
#define PHPMYADMIN_DATABASE_TO_EXPORT   ""

// GIT
#define GIT_LINK  "" // Your git repository (must be in privacy mode for safety reasons) for example: https://github.com/YourUsername/myproject.git

class Ftp2Git : public QObject
{

public:
    Ftp2Git();

    struct Node
    {
        Node         *parent;
        QList<Node*>  children;
        QUrlInfo      urlInfo; // Contains different informations about file like name, size aso.. http://doc.qt.io/archives/qt-4.8/qurlinfo.html
        QStringList   path;
        bool          alreadyShownInList = false; // Used to know if the actual node has been shown/written into the output file
    };

    void outputTreeToFile();
    void getSqlFileFromPhpMyAdmin();
    void runGit();

private:
    QFtp *ftp;
    QNetworkAccessManager *accessManager;
    QList<Node*> listOfNodes, listOfFolders;
    QHash<int, Node*>   hashList;
};

#endif // FTPMANAGER_H
