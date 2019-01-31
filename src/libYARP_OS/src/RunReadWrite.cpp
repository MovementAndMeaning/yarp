// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
* Copyright (C) 2007-2009 RobotCub Consortium
* CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
*/

#include <stdio.h>
#include <signal.h>
#include <yarp/os/Time.h>
#include <yarp/os/Bottle.h>
#include <yarp/os/Property.h>
#include <yarp/os/impl/RunReadWrite.h>

#if !defined(WIN32)
//#include <sys/wait.h>
//#include <errno.h>
//#include <string.h>
//#include <stdlib.h>
#endif

/////////////////////////////////////

int RunWrite::loop(yarp::os::ConstString &uuid)
{
    RUNLOG("<<<loop()")

    UUID=uuid;

    wPortName=uuid+"/stdout";

    if (!wPort.open(wPortName.c_str()))
    {
        RUNLOG("RunWrite: could not open output port")
        fprintf(stderr,"RunWrite: could not open output port\n");
        return 1;
    }

    char txt[2048];

    while (mRunning)
    {
        if (fgets(txt,2048,stdin)==0 || ferror(stdin) || feof(stdin)) break;

        if (!mRunning) break;

        yarp::os::Bottle bot;
        bot.addString(txt);
        wPort.write(bot);
    }        

    RUNLOG(">>>loop()")

    return 0;
}

///////////////////////////////////////////

int RunRead::loop(yarp::os::ConstString& uuid)
{
    RUNLOG("<<<loop()")

    UUID=uuid;

    rPortName=uuid+"/stdin";

    if (!rPort.open(rPortName.c_str()))
    {
        RUNLOG("RunRead: could not open input port")
        fprintf(stderr,"RunRead: could not open input port\n");
        return 1;
    }

    while (mRunning)
    {
        yarp::os::Bottle bot;
        if (!rPort.read(bot,true))
        {   
            RUNLOG("!rPort.read(bot,true)")
            break;
        }

        if (!mRunning) break;

        if (bot.size()==1)
        {
            printf("%s",bot.get(0).asString().c_str());
        }
        else
        {
            printf("%s\n", bot.toString().c_str());
        }

        fflush(stdout);
    }

    rPort.close();

    RUNLOG(">>>loop()")

    return 0;
}

///////////////////////////////////////////

int RunReadWrite::loop(yarp::os::ConstString &uuid)
{
    RUNLOG("<<<loop()")

    UUID=uuid;

    RUNLOG(uuid.c_str())

    wPortName=uuid+"/stdio:o";
    rPortName=uuid+"/stdio:i";

    if (!rPort.open(rPortName.c_str()))
    {
        RUNLOG("RunReadWrite: could not open input port") 
        fprintf(stderr,"RunReadWrite: could not open input port\n");
        return 1;
    }

    yarp::os::ContactStyle style;
    style.persistent=true;

    yarp::os::Network::connect((uuid+"/stdout").c_str(),rPortName.c_str(),style);
    
    #if !defined(WIN32)
    if (getppid()!=1)
    #endif
    {
        RUNLOG("start()")
        start();

        while (mRunning)
        {
            #if !defined(WIN32)
            if (getppid()==1) break;
            #endif
        
            yarp::os::Bottle bot;

            if (!rPort.read(bot,true))
            {
                RUNLOG("!rPort.read(bot,true)")
                break;
            }

            if (!mRunning) break;

            #if !defined(WIN32)
            if (getppid()==1) break;
            #endif
        
            if (bot.size()==1)
            {
                printf("%s",bot.get(0).asString().c_str());
            }
            else
            {
                printf("%s\n",bot.toString().c_str());
            }

            fflush(stdout);
        }

        rPort.close();

        wPort.close();

#if defined(WIN32)
        ::exit(0);
#else
        int term_pipe[2];
        int warn_suppress=pipe(term_pipe);
        dup2(term_pipe[0],STDIN_FILENO);
        FILE* file_term_pipe=fdopen(term_pipe[1],"w");
        fprintf(file_term_pipe,"SHKIATTETE!\n");
        fflush(file_term_pipe);
        fclose(file_term_pipe);
#endif
    }        

    RUNLOG(">>>loop()")

    return 0;
}

void RunReadWrite::run()
{
    char txt[2048];

    RUNLOG("<<<run()")

    if (!wPort.open(wPortName.c_str()))
    {
        RUNLOG("RunReadWrite: could not open output port")
        fprintf(stderr,"RunReadWrite: could not open output port\n");
        return;
    }

    yarp::os::ContactStyle style;
    style.persistent=true;

    yarp::os::Network::connect(wPortName.c_str(),(UUID+"/stdin").c_str(),style);

    while (mRunning)
    {
        RUNLOG("mRunning")

        #if !defined(WIN32)
        if (getppid()==1) break;
        #endif

        if (fgets(txt,2048,stdin)==0 || ferror(stdin) || feof(stdin)) break;

        RUNLOG(txt)

        #if !defined(WIN32)
        if (getppid()==1) break;
        #endif

        if (!mRunning) break;

        RUNLOG(txt)

        yarp::os::Bottle bot;
        bot.addString(txt);

        RUNLOG("<<<wPort.write(bot)")
        wPort.write(bot);
        RUNLOG(">>>wPort.write(bot)")
    }

    RUNLOG(">>>run()")
}

