#ifndef UNICORN_SESSION_H_
#define UNICORN_SESSION_H_
#include <lastfm/XmlQuery>
#include <lastfm/misc.h>
#include <lastfm/ws.h>
#include <QSharedData>

namespace unicorn {

class SessionData : public QSharedData
{
public:
    SessionData(): isSubscriber( false ){}
    ~SessionData();

    QString username;
    QString sessionKey;
    bool isSubscriber;
    bool remember;
};

class Session
{
public:
    Session();
    Session( const Session& other );
    Session( QNetworkReply* reply ) throw( lastfm::ws::Error );

    bool isValid() const
    {
        return d;
    }

    void setRememberSession( bool b );

    QString username() const;
    QString sessionKey() const;

    bool isSubscriber() const;

    static QNetworkReply* 
    getMobileSession( const QString& username, const QString& password )
    {
        QMap<QString, QString> params;
        params["method"] = "auth.getMobileSession";
        params["username"] = username;
        params["authToken"] = lastfm::md5( (username + lastfm::md5( password.toUtf8() ) ).toUtf8() );
        return lastfm::ws::post( params );
    }

    QDataStream& write( QDataStream& out ) const
    {
        QMap<QString, QString> data;
        data[ "username" ] = d->username;
        data[ "sessionkey" ] = d->sessionKey;
        data[ "subscriber" ] = d->isSubscriber ? "1" : "0";
        data[ "remember" ] = d->remember ? "1" : "0";
        out << data;
        return out;
    }

    QDataStream& read( QDataStream& in )
    {
        QMap<QString, QString> data;
        in >> data;
        if( !d ) 
            d = new SessionData();

        init( data[ "username" ], data[ "sessionkey" ], data[ "subscriber" ] == "1");
        d->remember = data[ "remember" ] == "1";
        
        return in;
    }

protected:
    void init( const QString& username, const QString& sessionKey, const bool isSubscriber );

private:
    QExplicitlySharedDataPointer<SessionData> d;
    QString m_prevUsername;
};

}

QDataStream& operator<<( QDataStream& out, const unicorn::Session& s );
QDataStream& operator>>( QDataStream& in, unicorn::Session& s );

#endif //UNICORN_SESSION_H_
