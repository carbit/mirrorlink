#ifndef MIRRORLINKCLIENT_H
#define MIRRORLINKCLIENT_H

#include <QObject>

class MirrorLinkClient : public QObject
{
	Q_OBJECT
public:
	explicit MirrorLinkClient(QObject *parent = 0);
	~MirrorLinkClient();

signals:

public slots:
	void onStart(QString ip, qint16 port, QString path);
	void onStop();
	void onLaunch(qint32 appid);

private:
	struct remote_server *m_server;
};

#endif // MIRRORLINKCLIENT_H