/***************************************************************************
                          kernel.cpp  -  description
                             -------------------
    begin                : 24/02/2004
    copyright            : (C) 1999 by Fran�ois Dupoux
                                 (C) 2003 Andras Mantia <amantia@kde.org>
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

// app includes
#include "apistruct.h"
#include "kexpression.h"
#include "kernel.h"
#include "resource.h"
#include "kfilereplacedoc.h"
#include "kfilereplaceview.h"
#include "kfilereplacepart.h"
#include "kconfirmdlg.h"
#include "kfilereplacelib.h"

// KDE includes
#include <kapplication.h>
#include <kdebug.h>
#include <kmessagebox.h>

// Qt includes
#include <qdir.h>
#include <qfileinfo.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qregexp.h>
#include <qlistview.h>

// Standard includes
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#define STATFS statvfs
#else
#define STATFS statfs
#endif
#include <sys/stat.h>

void *Kernel::replaceThread(RepDirArg* r)
{
  int nRes;
  
  g_bThreadRunning = true;

  kdDebug(23000) << "Starting replaceDirectory..." << endl;
  // Call another function to make easier to verify Thread Variables
  nRes = replaceDirectory(r->szDir, r, true); // true --> replace
  kdDebug(23000) << "replaceDirectory exited..." << endl;

  // The thread always finished here: success or error
  g_nFilesRep = nRes; // Number of replaced files
  g_bThreadRunning = false;

  return 0;
}


void *Kernel::searchThread(RepDirArg* r)
{

  int nRes;
  
  g_bThreadRunning = true;

  // Call another function to make easier to verify Thread Variables
  nRes = replaceDirectory(r->szDir, r, false); // false --> search

  // The thread always finished here: success or error
  g_nFilesRep = nRes; // Number of replaced files
  g_bThreadRunning = false;

  return 0;
}


int Kernel::replaceDirectory(const QString& szDir, RepDirArg* argu, bool bReplace)
{
  QString strDirpath;
  QString strBackup;
  QFileInfo fiNew;
  int nRes;
  int nNbRepFiles = 0;
  int nNeedReplace; // Not NULL if the file need to be replaced
  int nNbReplacements; // Nb Rep made in a call to replaceFile
  uint nDiskFreeSpace;
  int nConfirm = 0;
  QListViewItem *lvi;
  bool bAllStringsFound;
  

  //KFileReplaceApp *appKFR;
  //appKFR = (KFileReplaceApp *) (argu->mainwnd);
  QDir dir;
  int nFlags = QDir::Files | QDir::Readable | QDir::NoSymLinks;
  if (!argu->bIgnoreHidden)
    nFlags |= QDir::Hidden;

  // What type of files we must lis
  dir.setFilter(nFlags);
  dir.setPath(szDir);
  dir.setNameFilter(argu->szFilter);

  // 0. -*-*-*-*-*-*-*-*-*- Check it's a valid directory -*-*-*-*-*-*-*-*-*-
  if (!dir.isReadable() || !dir.exists())
    {
      g_szErrMsg = i18n("<qt>Cannot access directory <b>%1</b>.").arg(szDir);
      return -1;
    }

  // 1. -*-*-*-*-*-*-*-*-*- First, list all files -*-*-*-*-*-*-*-*-*-
  unsigned int i;
  for (i=0; i < dir.count(); i++)
    {
      // Process event to make the GUI updated
      kapp->processEvents();

      // Check the Thread needn't to stop
      if (g_bThreadMustStop )
        {
          kdDebug(23000) << "STOP THREAD!" << endl;
          return -1;
        }

      QString strFileWritepath = KFileReplaceLib::instance()->formatFullPath(szDir, dir[i]) + "new";
      QString strFileReadpath = KFileReplaceLib::instance()->formatFullPath(szDir, dir[i]);

      QFileInfo fiOld;
      fiOld.setFile(strFileReadpath);

      // if the file dates & size are correct for options
      if ((dir[i].right(4) != ".old") && 
           hasFileGoodOwners(strFileReadpath, argu) && 
           isFileGoodSizeProperties(strFileReadpath, argu->bMinSize, argu->bMaxSize, argu->nMinSize, argu->nMaxSize) && 
           ((!argu->bMinDate  && !argu->bMaxDate) || isFileGoodDateProperties(strFileReadpath, argu->nTypeOfAccess, argu->bMinDate, argu->bMaxDate, argu->qdMinDate, argu->qdMaxDate)))
        {

          // Test read access for file
          QFileInfo fi(strFileReadpath);
          if (!fi.exists() || !fi.isReadable()) // We don't have access to the file
            argu->view->addFullItem(false, dir[i], szDir, fiOld.size(), 0, 0, g_szErrMsg);
          else
            {
                              // ***** IF SEARCHNIG *******
              if (!bReplace)
                {
                  // If there are strings to search
                  if (argu->qlvStrings->childCount() > 0)
                    {
                      // Add the item in the list, but without details
                      QString strTemp = KFileReplaceLib::instance()->formatSize(fiOld.size());
                      lvi = new QListViewItem(argu->qlvResult, dir[i], szDir, strTemp);
                      // Run the search operation
                      kdDebug(23000) << "begin CALL for searchFile()" << endl;
                      nRes = searchFile(lvi, strFileReadpath, nNbReplacements, &bAllStringsFound, argu, argu->bHaltOnFirstOccur);
                      kdDebug(23000) << "end CALL for searchFile(). nNbReplacements = " << nNbReplacements << endl;
                      if (nRes == 0)
                        {
                          // if all strings must be found, and have not be found
                          if (argu->bAllStringsMustBeFound && !bAllStringsFound)
                            {
                              delete lvi;
                            }
                          // Update Result ListView        if found
                          else
                            {
                              if (nNbReplacements > 0)
                                {
                                  nNbRepFiles++;
                                  // nNbReplacements must be 0 if options "Stop at first occurrence" is enabled
                                  argu->view->updateItem(lvi, true, fiOld.size(), nNbReplacements*(argu->bHaltOnFirstOccur==false));
                                }
                              else // No strings found
                                argu->qlvResult->takeItem(lvi); // Remove lvi from the list
                            }

                        }
                      else // (nRes == -1)
                        {
                          // Update Result ListView        if found
                          nNbRepFiles++;
                          lvi = argu->view->addFullItem(false, dir[i], szDir, fiOld.size(), 0, 0, g_szErrMsg);
                          argu->view->updateItem(lvi, true, fiOld.size(), 0);
                        }

                    }
                  else // if there are no strigns to search
                    {
                      nNbRepFiles++;
                      lvi = argu->view->addFullItem(true, dir[i], szDir, fiOld.size(), fiOld.size(), 0);
                      argu->view->updateItem(lvi, true, fiOld.size(), 0);
                    }

                }
              else // ******* IF REPLACING *******
                {
                  // Test read access for file
                  QFileInfo fInfo(strFileReadpath);
                  if (!fInfo.exists() || !fInfo.isReadable() || !fInfo.isWritable()) // We don't have access to the file
                  {
                    g_szErrMsg = i18n("<qt>Cannot access file <b>%1</b> for writing.</qt>").arg(strFileReadpath);
                    argu->view->addFullItem(false, dir[i], szDir, fiOld.size(), 0, 0, g_szErrMsg);
                  } else
                  {
                    kdDebug(23000) << QString("In (searchFile) to check the file need replace (%1)").arg( strFileReadpath) << endl;
                    nRes = searchFile(0, strFileReadpath, nNeedReplace, &bAllStringsFound, argu, true);
                    kdDebug(23000) << QString("Out (searchFile) to check the file need replace (%1)").arg( strFileReadpath) << endl;
                    if (nRes == -1)
                      return -1;

                    if (nNeedReplace && (!argu->bAllStringsMustBeFound || bAllStringsFound)) // Replace only if there are occurrences
                      {
                        if ((argu->bConfirmFiles ) && (!argu->bSimulation ))
                          {
                            QString strMess = i18n("<qt>Directory: %1<br>Path: %2<br>Do you want to replace strings inside <b>%3</b> ?</qt>").arg(szDir).arg(dir[i]).arg(strFileReadpath);
                            nConfirm = KMessageBox::questionYesNo(argu->mainwnd, strMess, i18n("Replace file confirmation"));
                          }

                        if ((!argu->bConfirmFiles  || nConfirm == KMessageBox::Yes) || (argu->bSimulation )) // if we must replace in this file
                          {
                            // Check there is enough free disk space
                            if (!argu->bSimulation ) // if not a simulation
                              {
                                nRes = diskFreeSpaceForFile(nDiskFreeSpace, strFileReadpath);
                                if (nRes != -1 && nDiskFreeSpace < fiOld.size())
                                  {
                                    g_szErrMsg = i18n("<qt>There is not enough disk free space to replace in the file <b>%1</b>.</qt>").arg(strFileReadpath);
                                    return -1;
                                  }
                              }

                            // Add the item in the list, but without details
                            QString strTemp = KFileReplaceLib::instance()->formatSize(fiOld.size());
                            lvi = new QListViewItem(argu->qlvResult, dir[i], szDir, strTemp);
                            if (lvi == 0) // Not enough memory
                              return -1;

                            // Run the replace operation
                            kdDebug(23000) << "In replaceFile " << strFileReadpath << endl;
                            nRes = replaceFile(lvi, szDir, strFileReadpath, strFileWritepath, nNbReplacements, argu);
                            kdDebug(23000) << "Out replaceFile " << strFileReadpath << endl;

                            if (nRes == replaceSuccess || nRes == replaceFileSkipped || nRes == replaceSkipDir) // If success
                              {
                                nNbRepFiles++;

                                // Update Result ListView
                                fiNew.setFile(strFileWritepath);
                                argu->view->updateItem(lvi, true, fiNew.size(), nNbReplacements);


                                if (!argu->bSimulation )
                                  {
                                    if (argu->bBackup) // Create a backup of the file if option is true
                                      {
                                        strBackup = KFileReplaceLib::instance()->formatFullPath(szDir, dir[i]) + QString(".old");
                                        nRes = ::unlink(strBackup.local8Bit()); // Delete OLD file if exists
                                        nRes = ::rename(strFileReadpath.local8Bit(), strBackup.local8Bit());
                                      }
                                    else // Delete the old file
                                      nRes = ::unlink(strFileReadpath.local8Bit());
                                    // Rename the new file into OldFileName
                                    nRes = ::rename(strFileWritepath.local8Bit(), strFileReadpath.local8Bit());
                                  }
                                if (nRes == replaceSkipDir) // end of replaceDirectory() ==> go to next dir
                                  return replaceSuccess;
                              }
                            else // If error
                              {
                                // delete file.new
                                if (!argu->bSimulation)
                                  ::unlink(strFileWritepath.local8Bit());

                                // update Result ListView
                                argu->view->updateItem(lvi, false, 0, 0, g_szErrMsg);

                                if (nRes == replaceError) // error in the file, but continue in current directory
                                  {
                                  }
                                else if (nRes == replaceCancel) // end of total operation
                                  return -1;
                              }
                          }
                      }
                  }

                }
          }

        }
    }

  // 2. -*-*-*-*-*-*-*-*-*- Second, list all dir -*-*-*-*-*-*-*-*-*-

  if (argu->bRecursive) // If we must explore sub directories
    {
      // What type of files we must find

      nFlags = QDir::Dirs | QDir::Readable | QDir::Executable;
      if (!argu->bIgnoreHidden)
        nFlags |= QDir::Hidden;
      if (!argu->bFollowSymLinks)
        nFlags |= QDir::NoSymLinks;

      dir.setPath(szDir);
      dir.setNameFilter("*");
      dir.setFilter(nFlags);

      for (i=0; i < dir.count(); i++)
        {
          if ((dir[i] != ".") && (dir[i] != ".."))
            {
              if (argu->bConfirmDirs && bReplace) // If doing a replace and dir confirm activated (do not confirm when searching)
                {
                  QString strMess = i18n("<qt>Directory: <b>%1</b><br>Full path: <b>%2/%3</b><br><br>Do you want to replace strings in files of this directory?</qt>").arg(dir[i]).arg(szDir).arg(dir[i]);
                  nConfirm = KMessageBox::questionYesNo(argu->mainwnd, strMess, i18n("Replace directory confirmation"));
                }

              if (!bReplace  || !argu->bConfirmDirs  || nConfirm == KMessageBox::Yes)
                {
                  strDirpath = KFileReplaceLib::instance()->formatFullPath(szDir, dir[i]);
                  nRes = replaceDirectory(strDirpath, argu, bReplace); // Use recursivity
                  if (nRes == -1) // If error
                    return -1; // Stop the operation
                  nNbRepFiles += nRes;
                }
            }
        }
    }

  return nNbRepFiles;
}


bool Kernel::isFileGoodSizeProperties(const QString& szFileName, bool bMinSize, bool bMaxSize, uint nMinSize, uint nMaxSize)
{

  // If Minimal Size Option is Checked
  QFileInfo fi;
  fi.setFile(szFileName);

  bool bCond = (bMinSize && fi.size() < nMinSize || bMaxSize && fi.size() > nMaxSize);

  return (!bCond);
}


bool Kernel::isFileGoodDateProperties(const QString& szFileName, int nTypeOfAccess, bool bMinDate, bool bMaxDate, QDate qdMinDate, QDate qdMaxDate)
{

  // If Minimal Size Option is Checked
  QFileInfo fi;
  fi.setFile(szFileName);
  QDate dateFiledate; // Date of the file we must to compare with dateLimit

  // Get the File Date
  if (nTypeOfAccess == 0) // Last WRITE date
    dateFiledate = fi.lastModified().date();
  if (nTypeOfAccess == 1) // Last READ date
    dateFiledate = fi.lastRead().date();

  if (bMinDate && dateFiledate < qdMinDate) // Check the Minimal Date (After ...)
    return false;

  if (bMaxDate && dateFiledate > qdMaxDate) // Check the Maximal Date (Before ...)
    return false;

  return true; // File is valid
}

int Kernel::replaceFile2(QListViewItem *lvi, const QString& folder, const QString& oldFile, const QString& newFile, int& replacementsNumber, RepDirArg* argu)
{
 Q_UNUSED(lvi);
 Q_UNUSED(folder);
 Q_UNUSED(oldFile);
 Q_UNUSED(newFile);
 Q_UNUSED(replacementsNumber);
 Q_UNUSED(argu);
 return 0;  
}

int Kernel::replaceFile(QListViewItem *lvi, const QString &szDir, const QString& szOldFile, const QString& szNewFile, int& nNbReplacements, RepDirArg* argu)
{

  int nFdOldFile=0, nFdNewFile=0; // File descriptors
  void *vBeginOldFile;
  char *cBeginOldFile; // Pointer to the begin of the file
  char *cOldPt; // Pointer to the current data
  int nRes;
  bool bRes;
  uint nOldFileSize;
  QListViewItem *lviCurItem;
  QListViewItem *lviFirst;
  int nItemPos;
  struct stat statFile;
  KExpression kjeSearch(argu->bCaseSensitive, argu->bWildcards, argu->bIgnoreWhitespaces, argu->cWildcardsWords, argu->cWildcardsLetters);
  int nMaxLen;
  int nRecursiveLength;
  QString strMess;
  int nConfirm=0;

  /*KFileReplaceApp *app;
  app = (KFileReplaceApp *) (argu->mainwnd);*/

  // Items of the string list
  int nReplaceCount[MaxStringToSearch];
  QString strOld[MaxStringToSearch];
  QString strNew[MaxStringToSearch];

  // 0. Init
  nNbReplacements = 0;
  QFileInfo fiOld(szOldFile);
  nOldFileSize = fiOld.size();

  // 1. Open files
  //TODO: Replace all direct file manipulation code with KIO/QT one
  QFile oldFile(szOldFile);
  if (!oldFile.open(IO_ReadOnly))
  {
    g_szErrMsg = i18n("<qt>Cannot open file <b>%1</b> for reading.</qt>").arg(szOldFile);
    return replaceError;
  }
  nFdOldFile = oldFile.handle();

  QFile newFile(szNewFile);
  if (!argu->bSimulation ) // if a real replace operation
    {

      if (!newFile.open(IO_ReadWrite | IO_Truncate))
        {
          g_szErrMsg = i18n("<qt>Cannot open file <b>%1</b> for writing.</qt>").arg(szNewFile);
          return replaceError;
        }
      nFdNewFile = newFile.handle();

      // 2. Put new file the access rights of the old file
      nRes = ::fstat(nFdOldFile, &statFile);
      if (nRes == -1)
        {
          g_szErrMsg = i18n("<qt>Cannot read the access rights for file<b>%1</b></qt>").arg(szOldFile);
          return replaceError;
        }

      nRes = ::fchmod(nFdNewFile, statFile.st_mode);
      if (nRes == -1)
        {
          g_szErrMsg = i18n("<qt>Cannot set the access rights for file<b>%1</b></qt>").arg(szNewFile);
          //return replaceError; // make bug with files on FAT
        }
    }

  // 3. Map files
  vBeginOldFile = ::mmap((caddr_t)0, nOldFileSize, PROT_READ, MAP_SHARED, nFdOldFile, 0);
  if ((caddr_t) vBeginOldFile == MAP_FAILED)
    {
      g_szErrMsg = i18n("<qt>Cannot map file <b>%1</b> for reading.").arg(szOldFile);
      oldFile.close();
      return replaceError;
    }

  cBeginOldFile = (char *) vBeginOldFile;
  cOldPt = cBeginOldFile;

  // 4. Copy strings to search/remplace into strings in memory
  nItemPos = 0;
  lviCurItem = lviFirst = argu->qlvStrings->firstChild();
  if (lviCurItem == NULL)
    {
      g_szErrMsg = i18n("Cannot list tree items.");
      return replaceError;
    }

  do
    {
      strOld[nItemPos] = lviCurItem->text(0);
      strNew[nItemPos] = lviCurItem->text(1);
      nReplaceCount[nItemPos] = 0;
      nItemPos++;
      lviCurItem = lviCurItem->nextSibling();
    } while(lviCurItem && lviCurItem != lviFirst);

  // 5. Replace strings  --------------------------------------------
  while ( ((cOldPt - (char *) cBeginOldFile) < (int)nOldFileSize)) // While not end of file
    {
      nMaxLen = nOldFileSize - (cOldPt - (char *) cBeginOldFile); // Do not search after end of file

      for (int i=0; i < argu->qlvStrings->childCount(); i++) // For all strings to search
        {
          nRecursiveLength = 0;

          bRes = kjeSearch.doesStringMatch(cOldPt, MIN(argu->nMaxExpressionLength,nMaxLen), strOld[i].utf8(), strOld[i].length(), true, &nRecursiveLength);

          if (bRes ) // String matches
            {
                                // Replace
              nNbReplacements++;
              (nReplaceCount[i])++; // Number of replacements for this string
              g_nStringsRep++;

              QString strReplace;

              if (argu->bWildcardsInReplaceStrings)
                {
                  QStringList strList;

                  kjeSearch.extractWildcardsContentsFromFullString(cOldPt, nRecursiveLength, strOld[i].utf8(), strOld[i].length(), &strList);
                  // Create the replace string which contains the text the wildcards were coding for in the search expression
                  kdDebug(23000) << QString("INITIAL: ****(%1)****").arg(strNew[i]) << endl;
                  strReplace = kjeSearch.addWildcardsContentToString(strNew[i].utf8(), strNew[i].length(), &strList);
                  if ((strReplace == QString::null) && (strNew[i].length()))
                    return replaceError;
                  kdDebug(23000) << QString("FINAL: ****(%1)****").arg(strReplace) << endl;
                }
              else
                {
                  strReplace = strNew[i];
                }

                                // If there are variables --> Copy the contents
              if (argu->bVariables)
                {
                  strReplace = kjeSearch.substVariablesWithValues(strReplace, szOldFile);
                }

                                // Confirmation ==> Ask to user
              if (argu->bConfirmStrings )
                {
                  QString strOldTxt;
                  int j;

                  // Format found text and replace text
                  for (j=0; cOldPt[j] && j < nRecursiveLength; j++)
                    strOldTxt.append(cOldPt[j]);

                  KConfirmDlg dlg;
                  dlg.setData(szOldFile, szDir, strOldTxt, strReplace);
                  nConfirm = dlg.exec();
                  if (nConfirm == KConfirmDlg::Yes)
                    strReplace = dlg.getReplaceString();
                  else if (nConfirm == KConfirmDlg::Cancel)
                    {
                      g_szErrMsg = i18n("Operation canceled.");
                      return replaceCancel;
                    }
                  else if (nConfirm == KConfirmDlg::SkipFile)
                    {
                      g_szErrMsg = i18n("File skipped.");
                      return replaceFileSkipped;
                    }
                  else if (nConfirm == KConfirmDlg::SkipDir)
                    {
                      g_szErrMsg = i18n("Directory skipped.");
                      return replaceSkipDir;
                    }
                }

                                // If need not confirmation, or if used agree ==> DO THE REPLACE
              if (!argu->bConfirmStrings  || nConfirm == KConfirmDlg::Yes)
                {
                  // Add detailed result in result list
                  argu->view->increaseStringCount(lvi, strOld[i], strNew[i], strReplace, cOldPt, nRecursiveLength, true);

                  // Write the new replace string
                  if (!argu->bSimulation )
                    {
                      nRes = ::write(nFdNewFile, strReplace.local8Bit(), strReplace.length());
                      if (nRes != (int)strReplace.length())
                        {
                          g_szErrMsg = i18n("Cannot write data.");
                          return replaceError;
                        }
                    }
                  cOldPt += nRecursiveLength; // The length of the string with the wildcards cotents
                  goto end_replace_pos; // Do not make other replace on this byte
                }
            }

        }

      // Searched Text not present: copy char
      if (!argu->bSimulation )
        {
          nRes = ::write(nFdNewFile, cOldPt, 1);
          if (nRes != 1)
            {
              g_szErrMsg = i18n("<qt>Cannot write data in <b>%1<b>.</qt>").arg(szNewFile);
              return replaceError;
            }
        }
      cOldPt++;

    end_replace_pos:;
    }

  // --------------------------------------------

  // Unamp files
#if defined(USE_SOLARIS)
  ::munmap(cBeginOldFile, nOldFileSize);
#else
  ::munmap(vBeginOldFile, nOldFileSize);
#endif

  // Close files
  oldFile.close();

  if (!argu->bSimulation )
    newFile.close();

  return replaceSuccess; // Success
}


int Kernel::searchFile(QListViewItem *lvi, const QString &szOldFile, int& nNbReplacements, bool *bAllStringsFound, RepDirArg* argu, bool bHaltOnFirstOccur)
{

  int nFdOldFile; // File descriptors
  void *vBeginOldFile;
  char *cBeginOldFile; // Pointer to the begin of the file
  char *cOldPt; // Pointer to the current data
  uint nOldFileSize;
  QListViewItem *lviCurItem;
  QListViewItem *lviFirst;
  int nItemPos;
  bool bRes;
  KExpression kjeSearch(argu->bCaseSensitive, argu->bWildcards, argu->bIgnoreWhitespaces, argu->cWildcardsWords, argu->cWildcardsLetters);
  int nMaxLen;
  int nNbStrings;
  int i, j; // for(;;)

  /*KFileReplaceApp *app;
  app = (KFileReplaceApp *) (argu->mainwnd);*/

  // Items of the string list
  int nReplaceCount[MaxStringToSearch];
  QString strOld[MaxStringToSearch];

  // 0. Init
  nNbReplacements = 0;
  *bAllStringsFound = false;
  QFileInfo fiOld(szOldFile);
  nOldFileSize = fiOld.size();
  nNbStrings = argu->qlvStrings->childCount();

  // 1. Open files
  QFile oldFile(szOldFile);
  if (!oldFile.open(IO_ReadOnly))
  {
    g_szErrMsg = i18n("<qt>Cannot open file <b>%1</b> for reading.</qt>").arg(szOldFile);
    return -1;
  }
  nFdOldFile = oldFile.handle();

  // Map files
  vBeginOldFile = ::mmap((caddr_t)0, nOldFileSize, PROT_READ, MAP_SHARED, nFdOldFile, 0);
  if ((caddr_t) vBeginOldFile == MAP_FAILED)
    {
      g_szErrMsg = i18n("<qt>Cannot map file <b>%1</b> for reading.</qt>").arg(szOldFile);
      oldFile.close();
      return -1;
    }

  cBeginOldFile = (char *) vBeginOldFile;
  cOldPt = cBeginOldFile;

  // Copy strings to search/replace into strings in memory
  nItemPos = 0;
  lviCurItem = lviFirst = argu->qlvStrings->firstChild();
  if (lviCurItem == NULL)
    {
      g_szErrMsg = i18n("Cannot list tree items.");
      return -1;
    }

  do
    {
      strOld[nItemPos] = lviCurItem->text(0);
      nReplaceCount[nItemPos] = 0;
      nItemPos++;
      lviCurItem = lviCurItem->nextSibling();
    } while(lviCurItem && lviCurItem != lviFirst);


  // --------------------------------------------
  while ( ((cOldPt - (char *) cBeginOldFile) < (int)nOldFileSize)) // While not end of file
    {
      int nRecursiveLength = 0;
      nMaxLen = nOldFileSize - (cOldPt - ((char *) cBeginOldFile)); // Do not search after end of file

      for (i=0; i < nNbStrings; i++) // For all strings to search
        {
          bRes = kjeSearch.doesStringMatch(cOldPt, MIN(argu->nMaxExpressionLength, nMaxLen), strOld[i].utf8(), strOld[i].length(), true, &nRecursiveLength);

          if (bRes ) // String matches
            {
              if (!(*bAllStringsFound )) // test if true now (with the new string found)
                {
                  bool bAllPresent = true;
                  for (j=0; j < nNbStrings; j++)
                    if (!nReplaceCount[j])
                      { 
                        bAllPresent = false;
                        break;
                      }
                  *bAllStringsFound = bAllPresent;
                }

              // If stop at first success (do not need to know how many found)
              if (bHaltOnFirstOccur && (*bAllStringsFound || !argu->bAllStringsMustBeFound) )
                {
                  nNbReplacements = 1;
#if defined(USE_SOLARIS)
                  ::munmap(cBeginOldFile, nOldFileSize);
#else
                  ::munmap(vBeginOldFile, nOldFileSize);
#endif
                  oldFile.close();
                  return 0; // Success
                }

              nNbReplacements++;
              (nReplaceCount[i])++; // Number of replacements for this string
              if (!bHaltOnFirstOccur)
                g_nStringsRep++;

              // Add detailed result in result list
              if (lvi)
                argu->view->increaseStringCount(lvi, strOld[i], "", "", cOldPt, nRecursiveLength, !bHaltOnFirstOccur);

              cOldPt += nRecursiveLength;
              goto end_search; // Do not make other search on this byte
            }
        }

      cOldPt++;
    end_search:;
    }

  // Unamp files
#if defined(USE_SOLARIS)
  ::munmap(cBeginOldFile, nOldFileSize);
#else
  ::munmap(vBeginOldFile, nOldFileSize);
#endif

  // Close files
  oldFile.close();

  return 0; // Success
}



int Kernel::diskFreeSpaceForFile(unsigned int& nAvailDiskSpace, const QString &szFilename)
{
  int nRes;
  struct STATFS fsInfo;

  nAvailDiskSpace = 0;

  nRes = STATFS(szFilename.local8Bit(), &fsInfo); //FIXME: replace with a QT/KDE function
  if (nRes == -1)
    return -1;

  nAvailDiskSpace = fsInfo.f_bavail * fsInfo.f_bsize;

  return 0;
}


bool Kernel::hasFileGoodOwners(const QString &szFile, RepDirArg *argu)
{
  QFileInfo fi;
  fi.setFile(szFile);

  // +++++++++++ if must test the user owner +++++++++++++
  if (argu->bOwnerUserBool)
    {
      if (argu->strOwnerUserType == "name")
        {
          if (argu->bOwnerUserMustBe ) // owner user name must be xxx
            {
              kdDebug(23000) << QString("(%1): owner user name must be %2").arg(szFile).arg(argu->strOwnerUserValue) << endl;
              if (fi.owner() != argu->strOwnerUserValue)
                return false;
            }
          else        // owner user name must NOT be xxx
            {
              kdDebug(23000) << QString("(%1): owner user name must not be %2").arg(szFile).arg(argu->strOwnerUserValue) << endl;
              if (fi.owner() == argu->strOwnerUserValue)
                return false;
            }

        }
      else if (argu->strOwnerUserType == "ID (number)")
        {
          if (argu->bOwnerUserMustBe) // owner user ID must be xxx
            {
              kdDebug(23000) << QString("(%1): owner user ID must be %2").arg(szFile).arg(argu->strOwnerUserValue) << endl;
              if (fi.ownerId() != argu->strOwnerUserValue.toULong())
                return false;
            }
          else        // owner user ID must NOT be xxx
            {
              kdDebug(23000) << QString("(%1): owner user ID must not be %2").arg(szFile).arg(argu->strOwnerUserValue) << endl;
              if (fi.ownerId() == argu->strOwnerUserValue.toULong())
                return false;
            }
        }
    }

  // +++++++++++ if must test the group owner +++++++++++++
  if (argu->bOwnerGroupBool)
    {
      if (argu->strOwnerGroupType == "name")
        {
          if (argu->bOwnerGroupMustBe ) // owner group name must be xxx
            {
              kdDebug(23000) << QString("(%1): owner group name must be %2").arg(szFile).arg(argu->strOwnerGroupValue) << endl;
              if (fi.group() != argu->strOwnerGroupValue)
                return false;
            }
          else        // owner group name must NOT be xxx
            {
              kdDebug(23000) << QString("(%1): owner group name must not be %2").arg(szFile).arg(argu->strOwnerGroupValue) << endl;
              if (fi.group() == argu->strOwnerGroupValue)
                return false;
            }

        }
      else if (argu->strOwnerGroupType == "ID (number)")
        {
          if (argu->bOwnerGroupMustBe ) // owner group ID must be xxx
            {
              kdDebug(23000) << QString("(%1): owner group ID must be %2").arg(szFile).arg(argu->strOwnerGroupValue) << endl;
              if (fi.groupId() != argu->strOwnerGroupValue.toULong())
                return false;
            }
          else        // owner user ID must NOT be xxx
            {
              kdDebug(23000) << QString("(%1): owner group ID must not be %2").arg(szFile).arg(argu->strOwnerGroupValue) << endl;
              if (fi.groupId() == argu->strOwnerGroupValue.toULong())
                return false;
            }
        }
    }

  return true;
}

