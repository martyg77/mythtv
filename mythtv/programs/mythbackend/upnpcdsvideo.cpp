// Program Name: upnpcdsvideo.cpp
//                                                                            
// Purpose - uPnp Content Directory Extention for MythVideo Videos
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcdsvideo.h"
#include "httprequest.h"
#include <qfileinfo.h>
#include <qregexp.h>
#include <qurl.h>
#include <qdir.h>
#include <limits.h>
#include "util.h"

#define LOC QString("UPnpCDSVideo: "); 
#define LOC_WARN QString("UPnpCDSVideo, Warning: "); 
#define LOC_ERR QString("UPnpCDSVideo, Error: "); 

UPnpCDSRootInfo UPnpCDSVideo::g_RootNodes[] = 
{
    {   "VideoRoot", 
        "*",
        "SELECT 0 as key, "
          "title as name, "
          "1 as children "
            "FROM upnpmedia "
            "%1 "
            "ORDER BY title DESC",
        "" }

};

int UPnpCDSVideo::GetBaseCount(void)
{
    int res;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT COUNT(*) FROM upnpmedia WHERE class = 'VIDEO' "
                    "AND parentid = :ROOTID");

    query.bindValue(":ROOTID", STARTING_VIDEO_OBJECTID);
    query.exec();

    res = query.value(0).toInt();

    return res;
}
int UPnpCDSVideo::g_nRootCount = 1;

//int UPnpCDSVideo::g_nRootCount;
//= sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSVideo::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]); 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetTableName( QString sColumn )
{
    return "upnpmedia";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetItemListSQL( QString sColumn )
{
    return "SELECT intid, title, filepath, " \
       "itemtype, itemproperties, parentid, "\
           "coverart FROM upnpmedia WHERE class = 'VIDEO'";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int nVideoID = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE class = 'VIDEO' AND intid=:VIDEOID " )
                                                    .arg( GetItemListSQL( ) );

    query.prepare( sSQL );

    query.bindValue( ":VIDEOID", (int)nVideoID    );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::AddItem( const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{
    int            nVidID       = query.value( 0).toInt();
    QString        sTitle       = QString::fromUtf8(query.value( 1).toString());
    QString        sFileName    = QString::fromUtf8(query.value( 2).toString());
    QString        sItemType    = query.value( 3).toString();
    QString        sParentID    = query.value( 5).toString();
    QString        sCoverArt    = query.value( 6).toString();

    // VERBOSE(VB_UPNP,QString("ID = %1, Title = %2, fname = %3 sObjectId = %4").arg(nVidID).arg(sTitle).arg(sFileName).arg(sObjectId));

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------
    QString sServerIp = gContext->GetSetting("BackendServerIp"   );
    QString sPort     = gContext->GetSetting("BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle;

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    QString sURIParams = QString( "?Id=%1" )
                            .arg( nVidID );

    QString sId        = QString( "%1/item%2")
                            .arg( sObjectId )
                            .arg( sURIParams );

    sURIParams = QString( "/Id%1" ).arg( nVidID );
    sId        = QString( "%1/item%2").arg( sObjectId ).arg( sURIParams );

    if (sParentID == QString("%1").arg(STARTING_VIDEO_OBJECTID))
    {
        sParentID          = sObjectId;
    }
    else
    {
        sParentID          = QString( "%1/item/Id%2")
                            .arg( sObjectId )
                            .arg( sParentID );
    }

    QString sAlbumArtURI= QString( "%1GetVideoArt%2")
                        .arg( sURIBase   )
                        .arg( sURIParams );

    CDSObject *pItem = NULL;

    if (sItemType == "FOLDER") 
        pItem   = CDSObject::CreateStorageFolder( sId, sName, sParentID);
    else if (sItemType == "FILE" )
        pItem   = CDSObject::CreateVideoItem( sId, sName, sParentID );

    if (!pItem) 
    { 
        VERBOSE(VB_IMPORTANT, LOC_ERR + "AddItem(): " + 
        QString("sItemType has unknown type '%1'").arg(sItemType)); 
 
        return; 
    }

    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    pItem->SetPropValue( "genre"          , "[Unknown Genre]"     );
    pItem->SetPropValue( "actor"          , "[Unknown Author]"    );
    pItem->SetPropValue( "creator"        , "[Unknown Author]"    );
    pItem->SetPropValue( "album"          , "[Unknown Series]"    );

    if ((sCoverArt != "") && (sCoverArt != "No Cover"))
        pItem->SetPropValue( "albumArtURI"    , sAlbumArtURI);

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2").arg( m_sExtensionId )
                                                .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    QFileInfo fInfo( sFileName );
    QDateTime fDate = fInfo.lastModified();

    pItem->SetPropValue( "date", fDate.toString( "yyyy-MM-dd"));
    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.extension( FALSE ));
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;"
                                 "DLNA.ORG_FLAGS=0150000000000000000000000000"
                                 "0000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetVideo%2").arg( sURIBase   )
                                                .arg( sURIParams );

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );
    pRes->AddAttribute( "duration"  , "0:00:00.000"      );

}
