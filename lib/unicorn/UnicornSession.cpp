
#include <QApplication>
#include <QDebug>

#include <lastfm/User.h>
#include <lastfm/Auth.h>
#include <lastfm/XmlQuery.h>

#include "UnicornSession.h"
#include "UnicornSettings.h"

namespace unicorn {


QMap<QString, QString>
Session::lastSessionData()
{
    Settings s;
    QMap<QString, QString> sessionData;

    //use the Username setting or the first username if there have been any logged in previously
    QString username = s.value( "Username", QString() ).toString();

    if( !username.isEmpty() )
    {
        UserSettings us( username );

        sessionData[ "username" ] = username;
        const QString sk = us.value( "SessionKey", "" ).toString();

        if( !sk.isEmpty() )
            sessionData[ "sessionKey" ] = sk;
    }

    return sessionData;
}

Session::Session( const QString& username, QString sessionKey )
{
    init( username, sessionKey );
    connect( qApp, SIGNAL( internetConnectionUp() ), SLOT( fetchInfo() ) );
}

Session::Session( QDataStream& dataStream )
{
    dataStream >> *this;
    connect( qApp, SIGNAL( internetConnectionUp() ), SLOT( fetchInfo() ) );
}

QNetworkReply*
Session::getToken()
{
    QMap<QString, QString> params;
    params["method"] = "auth.getToken";
    return lastfm::ws::get( params );
}

QNetworkReply*
Session::getSession( QString token )
{
    QMap<QString, QString> params;
    params["method"] = "auth.getSession";
    params["token"] = token;
    return lastfm::ws::post( params, false );
}

bool
Session::youRadio() const
{
    return m_youRadio;
}

bool
Session::youFreeTrial() const
{
    return m_youFreeTrial;
}

bool
Session::registeredRadio() const
{
    return m_registeredRadio;
}

bool
Session::registeredFreeTrial() const
{
    return m_registeredFreeTrial;
}

bool
Session::subscriberRadio() const
{
    return m_subscriberRadio;
}

bool
Session::subscriberFreeTrial() const
{
    return m_subscriberFreeTrial;
}

QString 
Session::sessionKey() const
{
    return m_sessionKey;
}

lastfm::User
Session::user() const
{
    return m_user;
}

void 
Session::init( const QString& username, const QString& sessionKey )
{
    Settings s;
    s.setValue( "Username", username );

    m_user.setName( username );
    m_sessionKey = sessionKey;

    UserSettings us( username );
    m_user.setName( username );
    m_user.setScrobbleCount( us.value( "ScrobbleCount", 0 ).toInt() );
    m_user.setDateRegistered( us.value( "DateRegistered", QDateTime() ).toDateTime() );
    m_user.setRealName( us.value( "RealName", "" ).toString() );
    m_user.setIsSubscriber( us.value( UserSettings::subscriptionKey(), false ).toBool() );
    m_user.setType( static_cast<lastfm::User::Type>( us.value( "Type", lastfm::User::TypeUser ).toInt() ) );

    QList<QUrl> imageUrls;
    int imageCount = us.beginReadArray( "ImageUrls" );

    for ( int i = 0; i < imageCount; i++ )
    {
        us.setArrayIndex( i );
        imageUrls.append( us.value( "Url", QUrl() ).toUrl() );
    }

    us.endArray();

    m_user.setImages( imageUrls );

    if ( sessionKey.isEmpty() )
        Q_ASSERT( false );
    else
        us.setValue( "SessionKey", sessionKey );

    us.beginGroup( "Session" );
    m_youRadio = us.value( "youRadio", false ).toBool();
    m_youFreeTrial = us.value( "youFreeTrial", false ).toBool();
    m_registeredRadio = us.value( "registeredRadio", false ).toBool();
    m_registeredFreeTrial = us.value( "registeredFreeTrial", false ).toBool();
    m_subscriberRadio = us.value( "subscriberRadio", false ).toBool();
    m_subscriberFreeTrial = us.value( "subscriberFreeTrial", false ).toBool();
    us.endGroup();

    fetchInfo();
}

void
Session::fetchInfo()
{
    qDebug() << "fetching user info";
    lastfm::ws::Username = m_user.name();
    lastfm::ws::SessionKey = m_sessionKey;
    connect( lastfm::User::getInfo(), SIGNAL( finished() ), SLOT( onUserGotInfo() ) );
    connect( lastfm::Auth::getSessionInfo(), SIGNAL(finished()), SLOT(onAuthGotSessionInfo()) );
}

void
Session::onUserGotInfo()
{
    QNetworkReply* reply = ( QNetworkReply* )sender();

    XmlQuery lfm;

    if ( lfm.parse( reply ) )
    {
        lastfm::User user( lfm["user"] );

        m_user = user;
        cacheUserInfo( m_user );

        emit userInfoUpdated( m_user );
    }
    else
    {
        qDebug() << lfm.parseError().message() << lfm.parseError().enumValue();
    }
}

void
Session::onAuthGotSessionInfo()
{
    XmlQuery lfm;

    if ( lfm.parse( static_cast<QNetworkReply*>( sender() ) ) )
    {
        qDebug() << lfm;

        XmlQuery you = lfm["application"]["radioPermission"]["user type=you"];
        m_youRadio = you["radio"].text() != "0";
        m_youFreeTrial = you["freetrial"].text() != "0";

        XmlQuery registered = lfm["application"]["radioPermission"]["user type=registered"];
        m_registeredRadio = registered["radio"].text() != "0";
        m_registeredFreeTrial = registered["freetrial"].text() != "0";

        XmlQuery subscriber = lfm["application"]["radioPermission"]["user type=subscriber"];
        m_subscriberRadio = subscriber["radio"].text() != "0";
        m_subscriberFreeTrial = subscriber["freetrial"].text() != "0";

        bool isSubscriber = lfm["application"]["session"]["subscriber"].text() != "0";
        m_user.setIsSubscriber( isSubscriber );

        // fix the wrongness
        m_youRadio = isSubscriber ? m_subscriberRadio : m_registeredRadio;
        m_youFreeTrial = isSubscriber ? m_subscriberFreeTrial : m_registeredFreeTrial;

        cacheUserInfo( m_user ); // make sure the subscriber flag gets cached
        cacheSessionInfo( *this );

        emit sessionChanged( *this );
    }
    else
    {
        qDebug() << lfm.parseError().message() << lfm.parseError().enumValue();
    }
}

void
Session::cacheUserInfo( const lastfm::User& user )
{
    UserSettings us( user.name() );
    us.setValue( UserSettings::subscriptionKey(), user.isSubscriber() );
    us.setValue( "ScrobbleCount", user.scrobbleCount() );
    us.setValue( "DateRegistered", user.dateRegistered() );
    us.setValue( "RealName", user.realName() );
    us.setValue( "Type", user.type() );

    QList<User::ImageSize> sizes;
    sizes << User::SmallImage << User::MediumImage << User::LargeImage;

    us.beginWriteArray( "ImageUrls", sizes.count() );
    for ( int i = 0; i < sizes.count(); i++ )
    {
        us.setArrayIndex( i );
        us.setValue( "Url", user.imageUrl( sizes[ i ] ) );
    }
    us.endArray();
}

void
Session::cacheSessionInfo( const unicorn::Session& session )
{
    UserSettings us( session.user().name() );
    us.beginGroup( "Session" );
    us.setValue( "youRadio", session.m_youRadio );
    us.setValue( "youFreeTrial", session.m_youFreeTrial );
    us.setValue( "registeredRadio", session.m_registeredRadio );
    us.setValue( "registeredFreeTrial", session.m_registeredFreeTrial );
    us.setValue( "subscriberRadio", session.m_subscriberRadio );
    us.setValue( "subscriberFreeTrial", session.m_subscriberFreeTrial );
    us.endGroup();
}

QDataStream&
Session::write( QDataStream& out ) const
{
    QMap<QString, QString> data;
    data[ "username" ] = user().name();
    data[ "sessionkey" ] = m_sessionKey;
    out << data;
    return out;
}

QDataStream&
Session::read( QDataStream& in )
{
    QMap<QString, QString> data;
    in >> data;
    init( data[ "username" ], data[ "sessionkey" ] );
    return in;
}

}

QDataStream& operator<<( QDataStream& out, const unicorn::Session& s ){ return s.write( out ); }
QDataStream& operator>>( QDataStream& in, unicorn::Session& s ){ return s.read( in ); }

