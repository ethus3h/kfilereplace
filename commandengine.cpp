/***************************************************************************
                           commandengine.cpp  -  kfr commands feature class
                                      -------------------
    begin                : fri aug  13 15:29:46 CEST 2004

    copyright            : (C) 2004 Emiliano Gulmini
    email                : emi_barbarossa@yahoo.it
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
#include <qdatetime.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qdom.h>
//Added by qt3to4:
#include <Q3CString>

// KDE
#include <kuser.h>
#include <krandomsequence.h>
#include <k3process.h>

// local
#include "commandengine.h"

QString CommandEngine::datetime(const QString& opt, const QString& arg)
{
  Q_UNUSED(arg);
  if(opt == "iso")
    return QDateTime::currentDateTime().toString(Qt::ISODate);
  if(opt == "local")
    return QDateTime::currentDateTime().toString(Qt::LocalDate);
  return QString();
}

QString CommandEngine::user(const QString& opt, const QString& arg)
{
  Q_UNUSED(arg);
  KUser u;
  if(opt == "uid")
    return QString::number(u.uid(),10);
  if(opt == "gid")
    return QString::number(u.gid(),10);
  if(opt == "loginname")
    return u.loginName();
  if(opt == "fullname")
    return u.fullName();
  if(opt == "homedir")
    return u.homeDir();
  if(opt == "shell")
    return u.shell();
  return QString();
}

QString CommandEngine::loadfile(const QString& opt, const QString& arg)
{
  Q_UNUSED(arg);

  QFile f(opt);
  if(!f.open(QIODevice::ReadOnly)) return QString();

  QTextStream t(&f);

  QString s = t.readAll();

  f.close();

  return s;
}

QString CommandEngine::empty(const QString& opt, const QString& arg)
{
  Q_UNUSED(opt);
  Q_UNUSED(arg);
  return "";
}

QString CommandEngine::mathexp(const QString& opt, const QString& arg)
{
  /* We will use bc 1.06 by Philip A. Nelson <philnelson@acm.org> */
  //Q_UNUSED(opt);
  Q_UNUSED(arg);

  QString tempOpt = opt;
  tempOpt.replace("ln","l");
  tempOpt.replace("sin","s");
  tempOpt.replace("cos","c");
  tempOpt.replace("arctan","a");
  tempOpt.replace("exp","e");
  
  QString program = "var=("+tempOpt+");print var";
  QString script = "echo '"+program+"' | bc -l;";

  K3Process* proc = new K3Process();

  proc->setUseShell(true);

  *(proc) << script;

   connect(proc, SIGNAL(receivedStdout(K3Process*,char*,int)), this, SLOT(slotGetScriptOutput(K3Process*,char*,int)));
   connect(proc, SIGNAL(receivedStderr(K3Process*,char*,int)), this, SLOT(slotGetScriptError(K3Process*,char*,int)));
   connect(proc, SIGNAL(processExited(K3Process*)), this, SLOT(slotProcessExited(K3Process*)));

  //Through slotGetScriptOutput, m_processOutput contains the result of the K3Process call
   if(!proc->start(K3Process::Block, K3Process::All))
     {
       return QString();
     }
   else
     {
       proc->wait();
     }
   delete proc;

   QString tempbuf = m_processOutput;
   m_processOutput = QString();

   return tempbuf;

}

QString CommandEngine::random(const QString& opt, const QString& arg)
{
  Q_UNUSED(arg);
  long seed;
  if(opt.isEmpty())
    {
      QDateTime dt;
      seed = dt.toTime_t();
    }
  else
    seed = opt.toLong();

  KRandomSequence seq(seed);
  return QString::number(seq.getLong(1000000),10);
}

QString CommandEngine::stringmanip(const QString& opt, const QString& arg)
{
  Q_UNUSED(opt);
  Q_UNUSED(arg);
  return "";
}

QString CommandEngine::variableValue(const QString &variable)
{
  QString s = variable;

  s.remove("[$").remove("$]").remove(" ");

  if(s.contains(":") == 0)
    return variable;
  else
    {
      QString leftValue = s.section(":",0,0),
              midValue = s.section(":",1,1),
              rightValue = s.section(":",2,2);

      QString opt = midValue;
      QString arg = rightValue;

      if(leftValue == "stringmanip")
        return stringmanip(opt, arg);
      if(leftValue == "datetime")
        return datetime(opt, arg);
      if(leftValue == "user")
        return user(opt, arg);
      if(leftValue == "loadfile")
        return loadfile(opt, arg);
      if(leftValue == "empty")
        return empty(opt, arg);
      if(leftValue == "mathexp")
        return mathexp(opt, arg);
      if(leftValue == "random")
        return random(opt, arg);

      return variable;
    }
}

//SLOTS
void CommandEngine::slotGetScriptError(K3Process* proc, char* s, int i)
{
  Q_UNUSED(proc);
  Q_UNUSED(proc);
  Q3CString temp(s,i+1);
  if(temp.isEmpty() || temp == "\n") return;
}

void CommandEngine::slotGetScriptOutput(K3Process* proc, char* s, int i)
{
  Q_UNUSED(proc);
  Q3CString temp(s,i+1);

  if(temp.isEmpty() || temp == "\n") return;

  m_processOutput += QString::fromLocal8Bit(temp);
}

void CommandEngine::slotProcessExited(K3Process* proc)
{
  Q_UNUSED(proc);
}

#include "commandengine.moc"
