#include "ftp2git.h"

Ftp2Git::Ftp2Git()
{
    ftp = new QFtp;

    // State of the ftp connection
    QObject::connect(ftp, &QFtp::stateChanged, [=](int state){
        switch(state){
        case QFtp::Unconnected:
            qDebug() << "[INFO]: There is no connection to the host" << endl;
            break;
        case QFtp::HostLookup:
            qDebug() << "[INFO]: A host name lookup is in progress" << endl;
            break;
        case QFtp::Connecting:
            qDebug() << "[INFO]: An attempt to connect to the host is in progress" << endl;
            break;
        case QFtp::Connected:
            qDebug() << "[INFO]: Connection to the host has been achieved" << endl;
            break;
        case QFtp::LoggedIn:
            qDebug() << "[INFO]: Connection and user login have been achieved" << endl;
            break;
        case QFtp::Closing:
            qDebug() << "[INFO]: The connection is closing down, but it is not yet closed. (The state will be Unconnected when the connection is closed.)" << endl;
            break;
        default:
            qDebug() << "YOUHOUUUUUU ! You broke it" << endl;
            break;
        }
    });

    // List each files into the actual path (called after ftp->list())
    QObject::connect(ftp, &QFtp::listInfo, [=](QUrlInfo i){
        Node *childNodePtr      = new Node;
        childNodePtr->parent    = listOfNodes.last();
        childNodePtr->urlInfo   = i;
        childNodePtr->path      = listOfNodes.last()->path + QStringList(i.name());

        if(i.isDir()){
            if(i.name() != "." && i.name() != ".."){
                listOfFolders.append(childNodePtr);
                listOfNodes.last()->children.append(childNodePtr);
            }
        }else{
            listOfNodes.last()->children.append(childNodePtr);

            int code = -1;

            if(QString(childNodePtr->path.at(0)).contains("/"))
                childNodePtr->path.first() = QString(childNodePtr->path.first()).remove(0, 1); // QDir().mkpath doesn't create folder if it begins with /. So we get ride of it

            if(QFileInfo(childNodePtr->path.join("/")).size() != i.size()){ // We only download the file when it has different size
                code = ftp->get(QString("/") + childNodePtr->path.join("/"));
            }

            hashList.insert(code, childNodePtr);
        }
    });

    // Last command is done (called after the last command is done, so for example when ftp->list() finishes)
    QObject::connect(ftp, &QFtp::done, [=](bool error){
        if(error)
            return;

        if(!listOfFolders.isEmpty()){
            listOfNodes.append(listOfFolders.first());
            ftp->list(this->listOfFolders.first()->path.join("/"));
            listOfFolders.removeFirst();

        }else{ // We are done, there is no more node work to do so we can do whatevever with it
            qDebug() << "Node done" << endl;
            outputTreeToFile();
            getSqlFileFromPhpMyAdmin();
            runGit();
        }
    });

    // Download and write file locally
    QObject::connect(ftp, &QFtp::commandFinished, [=](int id, bool error){
        if(hashList.value(id) != 0){ // If the hashList contains a value for id
            QStringList cpyPath = hashList.value(id)->path;
            cpyPath.removeLast();

            QDir().mkpath(cpyPath.join("/"));

            QFile file(hashList.value(id)->path.join("/"));
            if(!file.open(QIODevice::WriteOnly)){
                qDebug() << "Can't open file " << file.fileName() << endl;
                return;
            }

            file.write(ftp->readAll());
            file.close();
        }
    });


    Node *mainNode      = new Node;

    QUrlInfo            urlInfo;
    urlInfo.setName(FTP_FOLDER);
    urlInfo.setDir(true);

    mainNode->urlInfo  = urlInfo;
    mainNode->path     = QStringList(FTP_FOLDER);
    mainNode->parent   = nullptr;

    listOfNodes.append(mainNode);

    ftp->connectToHost(HOST);
    ftp->login(USERNAME, PASSWORD);
    ftp->cd(FTP_FOLDER);
    ftp->list();
}

void Ftp2Git::outputTreeToFile(){

    if(QFileInfo(TREEOUTPUT_FILENAME).exists())
        QFile(TREEOUTPUT_FILENAME).remove();

    QFile file(TREEOUTPUT_FILENAME);
    if(!file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append)){
        qDebug() << "impossible to open file " << TREEOUTPUT_FILENAME << endl;
        return;
    }

    Node *mainNode = listOfNodes.first();

    QTextStream textStream(&file);
    QStringList alignment;

    for(int i = 0; 1; ++i){

        if(i == mainNode->children.count()){ // Finished to travel the folder node

            mainNode->alreadyShownInList = true; // All the actual folder has been "travelled". We put the folder node has alreadyShown

            if(mainNode->parent == nullptr){ // If the parent of the actual folder is nullptr, it means we are on the first node (only the first node doesn't have a parent)
                break; // We stop the loop
            }else{
                mainNode = mainNode->parent; // We go "upward" on the folders hiearchy
                alignment.removeLast();
            }

            i = 0;
        }

        if(mainNode->children.at(i)->alreadyShownInList == false && mainNode->children.at(i)->urlInfo.isDir()){
            textStream << alignment.join("") << " -- " << mainNode->children.at(i)->urlInfo.name() << "\n";
            mainNode = mainNode->children.at(i);

            alignment << "\t";

            i = -1;

        }else if(mainNode->children.at(i)->alreadyShownInList == false && mainNode->children.at(i)->urlInfo.isDir() == false){
            textStream << alignment.join("") << " - " << mainNode->children.at(i)->urlInfo.name() << "\n";
            mainNode->children.at(i)->alreadyShownInList = true;
        }
    }

    qDebug() << "Node tree outputed to file" << endl;

    file.close();
}

void Ftp2Git::getSqlFileFromPhpMyAdmin(){
    accessManager = new QNetworkAccessManager;

    // Let's look the actual phpMyAdmin html source
    QNetworkReply *getReply = accessManager->get(QNetworkRequest(QUrl(PHPMYADMIN_URL)));

    QObject::connect(getReply, &QNetworkReply::finished, [=](){

        // Let's take the token from the html source
        QRegularExpression re("<input type=\"hidden\" name=\"token\" value=\"(\\w+)\" />");
        QRegularExpressionMatch match = re.match(QString(getReply->readAll()));
        QString token = match.captured(1);

        if(!match.hasMatch()){
            qDebug() << "Error couldn't find any token on the html code of " << PHPMYADMIN_URL << " are you sure it is a phpmyadmin page (or maybe the page html structure has changed) ? " << endl;
            return;
        }

        // Let's connect to phpMyAdmin
        QNetworkRequest request(QUrl(PHPMYADMIN_URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // We are now connected and accessManager contains all the cookies. Let's export the database to database.sql and Voilà
        QNetworkReply *postReply = accessManager->post(request, QByteArray(QString("pma_username=%1&pma_password=%2&server=1&token=%3").arg(PHPMYADMIN_USER, PHPMYADMIN_PASSWORD, token).toStdString().c_str()));

        QObject::connect(postReply, &QNetworkReply::finished, [=](){


            QNetworkRequest request(QUrl(PHPMYADMIN_URL + QString("export.php")));
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");


            QNetworkReply *getSqlFile = accessManager->post(request, QByteArray(QString("token=%1&export_type=server&export_method=quick&quick_or_custom=custom&db_select%5B%5D=%2&output_format=sendit&filename_template=%40SERVER%40&remember_template=on&charset_of_file=utf-8&compression=none&what=sql&codegen_structure_or_data=data&codegen_format=0&csv_separator=%2C&csv_enclosed=%22&csv_escaped=%5C&csv_terminated=AUTO&csv_null=NULL&csv_structure_or_data=data&excel_null=NULL&excel_edition=win&excel_structure_or_data=data&htmlword_structure_or_data=structure_and_data&htmlword_null=NULL&json_structure_or_data=data&latex_caption=something&latex_structure_or_data=structure_and_data&latex_structure_caption=Structure+de+la+table+%40TABLE%40&latex_structure_continued_caption=Structure+de+la+table+%40TABLE%40+%28suite%29&latex_structure_label=tab%3A%40TABLE%40-structure&latex_relation=something&latex_comments=something&latex_mime=something&latex_columns=something&latex_data_caption=Contenu+de+la+table+%40TABLE%40&latex_data_continued_caption=Contenu+de+la+table+%40TABLE%40+%28suite%29&latex_data_label=tab%3A%40TABLE%40-data&latex_null=%5Ctextit%7BNULL%7D&mediawiki_structure_or_data=data&ods_null=NULL&ods_structure_or_data=data&odt_structure_or_data=structure_and_data&odt_relation=something&odt_comments=something&odt_mime=something&odt_columns=something&odt_null=NULL&pdf_report_title=&pdf_structure_or_data=data&php_array_structure_or_data=data&sql_include_comments=something&sql_header_comment=&sql_compatibility=NONE&sql_structure_or_data=structure_and_data&sql_procedure_function=something&sql_create_table_statements=something&sql_if_not_exists=something&sql_auto_increment=something&sql_backquotes=something&sql_type=INSERT&sql_insert_syntax=both&sql_max_query_size=50000&sql_hex_for_blob=something&sql_utc_time=something&texytext_structure_or_data=structure_and_data&texytext_null=NULL&yaml_structure_or_data=data").arg(token, PHPMYADMIN_DATABASE_TO_EXPORT).toStdString().c_str()));

            QObject::connect(getSqlFile, &QNetworkReply::finished, [=](){
                QByteArray data = getSqlFile->readAll();

                QFile htmlOutput("database.sql");

                if(!htmlOutput.open(QIODevice::WriteOnly)){
                    qDebug() << "Couldn't open " << htmlOutput.fileName() << endl;
                    return;
                }

                htmlOutput.write(data);
                htmlOutput.close();

                qDebug() << "Sql database downloaded" << endl;
            });

        });

    });
}

void Ftp2Git::runGit(){
    QProcess *gitProcess = new QProcess;
    gitProcess->setWorkingDirectory(QDir::currentPath());

    bool needToPushToRemote = false;

    QObject::connect(gitProcess, &QProcess::readyRead, [=](){
        QByteArray allData = gitProcess->readAll();
        qDebug() << "[PROCESS OUTPUT]: " << allData << endl;
    });

    // Does the .git folder exists on the actual path ? If not, let's init git repository
    if(!QDir(".git").exists()){
        gitProcess->execute("git init");
        needToPushToRemote = true;
    }

    gitProcess->execute("git add .");
    gitProcess->execute(QString("git commit -m \"%1\"").arg(QDateTime::currentDateTime().toString("dd MMM yyyy  HH:mm")));

    if(needToPushToRemote){
        gitProcess->execute(QString("git remote add origin %1").arg(GIT_LINK));
    }

    gitProcess->execute("git push -u origin master");

    qDebug() << "Pushed all modifications" << endl;

}
