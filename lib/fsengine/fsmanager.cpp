/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/


#include "fsmanager.h"
#include "fsengine.h"
#include "bsa.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QLayout>
#include <QListView>
#include <QPushButton>
#include <QSettings>
#include <QStringListModel>

#include <options.h>

//! Global BSA file manager
static FSManager *theFSManager = NULL;
// see fsmanager.h
FSManager* FSManager::get() 
{
	if (theFSManager == NULL)
		theFSManager = new FSManager();
	return theFSManager;
}

// see fsmanager.h
QList <FSArchiveFile *> FSManager::archiveList()
{
	QList<FSArchiveFile *> archives;
	for ( FSArchiveHandler* an : get()->archives.values() ) {
		archives.append( an->getArchive() );
	}
	return archives;
}

// see fsmanager.h
FSManager::FSManager( QObject * parent )
	: QObject( parent ), automatic( false )
{
	QSettings cfg;
	QStringList list = cfg.value( "FSEngine/Archives", QStringList() ).toStringList();

	if ( list.size() == 1 && list.first() == "AUTO" )
	{
		automatic = true;
		list = autodetectArchives();
	}
	
	for ( const QString an : list )
	{
		if ( FSArchiveHandler * a = FSArchiveHandler::openArchive( an ) )
			archives.insert( an, a );
	}
}

// see fsmanager.h
FSManager::~FSManager()
{
	qDeleteAll( archives );
}

// see fsmanager.h
QStringList FSManager::regPathBSAList( QString regKey, QString dataDir )
{
	QStringList list;
	QSettings reg( regKey, QSettings::NativeFormat );
	QString dataPath = reg.value( "Installed Path" ).toString();
	if ( ! dataPath.isEmpty() )
	{
		if ( ! dataPath.endsWith( '/' ) && ! dataPath.endsWith( '\\' ) )
			dataPath += "/";
		dataPath += dataDir;
		QDir fs( dataPath );
		for ( const QString& fn : fs.entryList( { "*.bsa" }, QDir::Files ) )
		{
			list << dataPath + QDir::separator() + fn;
		}
	}
	return list;
}

QStringList FSManager::autodetectArchives()
{
	QStringList list;
	
#ifdef Q_OS_WIN32
	list << regPathBSAList( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Morrowind", "Data Files" );
	list << regPathBSAList( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Oblivion", "Data" );
	list << regPathBSAList( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Fallout3", "Data" );
	list << regPathBSAList( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\FalloutNV", "Data" );
	list << regPathBSAList( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Skyrim", "Data" );
#endif
	
	return list;
}

// see fsmanager.h
void FSManager::selectArchives()
{
	FSSelector select( this );
	select.exec();
}

// see fsmanager.h
FSSelector::FSSelector( FSManager * m )
	: QDialog(), manager( m )
{
	model = new QStringListModel( this );
	model->setStringList( manager->archives.keys() );
	
	view = new QListView( this );
	view->setModel( model );
	view->setEditTriggers( QListView::NoEditTriggers );
	
	chkAuto = new QCheckBox( this );
	chkAuto->setText( "Automatic Selection" );
	chkAuto->setChecked( manager->automatic );
	connect( chkAuto, SIGNAL( toggled( bool ) ), this, SLOT( sltAuto( bool ) ) );
	
	btAdd = new QPushButton( "Add", this );
	btAdd->setDisabled( manager->automatic );
	connect( btAdd, SIGNAL( clicked() ), this, SLOT( sltAdd() ) );
	
	btDel = new QPushButton( "Remove", this );
	btDel->setDisabled( manager->automatic );
	connect( btDel, SIGNAL( clicked() ), this, SLOT( sltDel() ) );

	btDelAll = new QPushButton( "Remove All", this );
	btDelAll->setDisabled( manager->automatic );
	connect( btDelAll, SIGNAL( clicked() ), this, SLOT( sltDelAll() ) );
	
	QGridLayout * grid = new QGridLayout( this );
	grid->addWidget( chkAuto, 0, 0, 1, 2 );
	grid->addWidget( view, 1, 0, 1, 3 );
	grid->addWidget( btAdd, 2, 0, 1, 1 );
	grid->addWidget( btDel, 2, 1, 1, 1 );
	grid->addWidget( btDelAll, 2, 2, 1, 1 );
}

FSSelector::~FSSelector()
{
	QSettings cfg;
	QStringList list( manager->automatic ? QStringList() << "AUTO" : manager->archives.keys() );
	cfg.setValue( "FSEngine/Archives", list );

	emit Options::get()->sigFlush3D();
}

void FSSelector::sltAuto( bool x )
{
	if ( x )
	{
		qDeleteAll( manager->archives );
		manager->archives.clear();
		
		for ( const QString an : manager->autodetectArchives() )
		{
			if ( FSArchiveHandler * a = FSArchiveHandler::openArchive( an ) )
			{
				manager->archives.insert( an, a );
			}
		}
		
		model->setStringList( manager->archives.keys() );
	}
	
	manager->automatic = x;
	
	btAdd->setDisabled( x );
	btDel->setDisabled( x );
	btDelAll->setDisabled( x );
}

void FSSelector::sltAdd()
{
	QStringList list = QFileDialog::getOpenFileNames( this, "Select resource files to add", QString(), "BSA (*.bsa)" );
	
	for ( const QString an : list )
	{
		if ( ! manager->archives.contains( an ) )
			if ( FSArchiveHandler * a = FSArchiveHandler::openArchive( an ) )
				manager->archives.insert( an, a );
	}
	
	model->setStringList( manager->archives.keys() );
}

void FSSelector::sltDel()
{
	QString an = view->currentIndex().data( Qt::DisplayRole ).toString();
	if ( FSArchiveHandler * a = manager->archives.take( an ) )
	{
		delete a;
	}
	
	model->setStringList( manager->archives.keys() );
}

void FSSelector::sltDelAll()
{
	qDeleteAll( manager->archives );
	manager->archives.clear();

	model->setStringList( QStringList() );
}
