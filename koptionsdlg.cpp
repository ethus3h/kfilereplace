/***************************************************************************
                          koptionsdlg.cpp  -  description
                             -------------------
    begin                : Tue Dec 28 1999
    copyright            : (C) 1999 by Fran�ois Dupoux
                           (C) 2004 Emiliano Gulmini <emi_barbarossa@yahoo.it>
    email                : dupoux@dupoux.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// QT
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qwhatsthis.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>

// KDE
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kapplication.h>
//#include <kdebug.h>

// local
#include "whatthis.h"
#include "koptionsdlg.h"


using namespace whatthisNameSpace;


KOptionsDlg::KOptionsDlg(RCOptions* info, QWidget *parent, const char *name) : KOptionsDlgS(parent,name,true)
{
  QString configName = locateLocal("config", "kfilereplacerc");
  m_config = new KConfig(configName);
  m_option = info;

  initGUI();

  connect(m_pbOK, SIGNAL(clicked()), this, SLOT(slotOK()));
  connect(m_pbDefault, SIGNAL(clicked()),this,SLOT(slotDefaults()));
  connect(m_chbBackup, SIGNAL(toggled(bool)), this, SLOT(slotChbBackup(bool)));
  connect(m_pbHelp, SIGNAL(clicked()), this, SLOT(slotHelp()));

  whatsThis();
}

KOptionsDlg::~KOptionsDlg()
{
}

void KOptionsDlg::slotOK()
{
  accept();
}

void KOptionsDlg::initGUI()
{
  m_config->sync();
  m_config->setGroup("Notification Messages");
  m_option->m_notifyOnErrors = m_config->readBoolEntry(rcNotifyOnErrors, true);

  m_chbCaseSensitive->setChecked(m_option->m_caseSensitive);
  m_chbRecursive->setChecked(m_option->m_recursive);

  bool enableBackup = m_option->m_backup;

  m_chbBackup->setChecked(enableBackup);
  m_leBackup->setEnabled(enableBackup);
  m_tlBackup->setEnabled(enableBackup);

  m_leBackup->setText(m_option->m_backupExtension);

  m_chbVariables->setChecked(m_option->m_variables);
  m_chbRegularExpressions->setChecked(m_option->m_regularExpressions);
  m_chbHaltOnFirstOccurrence->setChecked(m_option->m_haltOnFirstOccur);
  m_chbFollowSymLinks->setChecked(m_option->m_followSymLinks);
  m_chbIgnoreHidden->setChecked(m_option->m_ignoreHidden);
  m_chbIgnoreFiles->setChecked(m_option->m_ignoreFiles);
  m_chbNotifyOnErrors->setChecked(m_option->m_notifyOnErrors);
}

void KOptionsDlg::saveRCOptions()
{
  m_option->m_caseSensitive = m_chbCaseSensitive->isChecked();
  m_option->m_recursive = m_chbRecursive->isChecked();
  QString backupExt = m_leBackup->text();
  m_option->m_backup = (m_chbBackup->isChecked() && !backupExt.isEmpty());
  m_option->m_backupExtension = backupExt;
  m_option->m_variables = m_chbVariables->isChecked();
  m_option->m_regularExpressions = m_chbRegularExpressions->isChecked();
  m_option->m_haltOnFirstOccur = m_chbHaltOnFirstOccurrence->isChecked();
  m_option->m_followSymLinks = m_chbFollowSymLinks->isChecked();
  m_option->m_ignoreHidden = m_chbIgnoreHidden->isChecked();
  m_option->m_ignoreFiles = m_chbIgnoreFiles->isChecked();
  m_option->m_notifyOnErrors = m_chbNotifyOnErrors->isChecked();

  m_config->setGroup("Notification Messages");
  m_config->writeEntry(rcNotifyOnErrors, m_option->m_notifyOnErrors);
  m_config->sync();
}

/** Set defaults values for all options of the dialog */
void KOptionsDlg::slotDefaults()
{
  m_chbCaseSensitive->setChecked(CaseSensitiveOption);
  m_chbRecursive->setChecked(RecursiveOption);
  m_chbHaltOnFirstOccurrence->setChecked(StopWhenFirstOccurenceOption);

  m_chbFollowSymLinks->setChecked(FollowSymbolicLinksOption);
  m_chbIgnoreHidden->setChecked(IgnoreHiddenOption);
  m_chbRegularExpressions->setChecked(RegularExpressionsOption);
  m_chbIgnoreFiles->setChecked(IgnoreFilesOption);

  QStringList bkList = QStringList::split(",",BackupExtensionOption,true);

  bool enableBackup = (bkList[0] == "true" ? true : false);

  m_chbBackup->setChecked(enableBackup);
  m_leBackup->setEnabled(enableBackup);
  m_tlBackup->setEnabled(enableBackup);

  m_leBackup->setText(bkList[1]);

  m_chbVariables->setChecked(VariablesOption);

  m_chbNotifyOnErrors->setChecked(NotifyOnErrorsOption);
}

void KOptionsDlg::slotChbBackup(bool b)
{
  m_leBackup->setEnabled(b);
  m_tlBackup->setEnabled(b);
}

void KOptionsDlg::whatsThis()
{
  // Create help QWhatsThis
  QWhatsThis::add(m_chbCaseSensitive, chbCaseSensitiveWhatthis);
  QWhatsThis::add(m_chbRecursive, chbRecursiveWhatthis);
  QWhatsThis::add(m_chbHaltOnFirstOccurrence, chbHaltOnFirstOccurrenceWhatthis);
  QWhatsThis::add(m_chbFollowSymLinks, chbFollowSymLinksWhatthis);
  QWhatsThis::add(m_chbIgnoreHidden, chbIgnoreHiddenWhatthis);
  QWhatsThis::add(m_chbIgnoreFiles, chbIgnoreFilesWhatthis);
  QWhatsThis::add(m_chbRegularExpressions, chbRegularExpressionsWhatthis);
  QWhatsThis::add(m_chbVariables, chbVariablesWhatthis);
  QWhatsThis::add(m_chbBackup, chbBackupWhatthis);
  QWhatsThis::add(m_chbConfirmStrings, chbConfirmStringsWhatthis);
}

#include "koptionsdlg.moc"
