// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
 * Copyright (C) 2013 iCub Facility
 * Authors: Paul Fitzpatrick
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 *
 */

#include <stdio.h>

#include <string>

#include "RosType.h"
#include "RosTypeCodeGenYarp.h"

#include <yarp/os/all.h>
#include <yarp/os/NetType.h>

#include <sys/stat.h>

#ifdef YARP_PRESENT
#  include <yarp/conf/system.h>
#endif
#ifdef YARP_HAS_ACE
#  include <ace/OS_NS_unistd.h>
#  include <ace/OS_NS_sys_wait.h>
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  ifndef ACE_OS
#    define ACE_OS
#  endif
#endif
#include <stdlib.h>

using namespace yarp::os;
using namespace std;

void show_usage() {
    printf("Usage:\n");
    printf("\n  yarpidl_rosmsg <Foo>.msg\n");
    printf("    Translates a ROS-format .msg file to a YARP-compatible .h file\n");
    printf("\n  yarpidl_rosmsg <package>/<Foo>\n");
    printf("    Calls 'rosmsg' to find type Foo, then makes a .h file for it\n");
    printf("\n  yarpidl_rosmsg --web true <package>/<Foo>\n");
    printf("    Allow YARP to look up missing types on ROS website\n");
    printf("\n  yarpidl_rosmsg --out <dir> <Foo>.msg\n");
    printf("    Generates .h file in the specified directory\n");
    printf("\n  yarpidl_rosmsg <Foo>.srv\n");
    printf("    Translates a ROS-format .srv file to a pair of YARP-compatible .h files\n");
    printf("    The classes generated for Foo.srv are Foo and FooReply.\n");
    printf("\n  yarpidl_rosmsg --name /name\n");
    printf("    Start up a service with the given port name for querying types.\n");
}

static void generateTypeMap1(RosType& t, ConstString& txt) {
    if (!t.isValid) return;
    RosType::RosTypes& lst = t.subRosType;
    if (lst.size()>0) {
        bool simple = true;
        for (size_t i=0; i<lst.size(); i++) { 
            if (lst[i].rosType != lst[0].rosType ||
                (!lst[i].isPrimitive) ||
                lst[i].isArray) {
                simple = false;
                break;
            }
            
        }
        if (!simple) {
            txt += " list ";
            txt += NetType::toString((int)lst.size());
            
            for (size_t i=0; i<lst.size(); i++) { 
                generateTypeMap1(lst[i],txt);
            }
        } else {
            txt += " vector ";
            txt += lst[0].rosType;
            txt += " ";
            txt += NetType::toString((int)lst.size());
            txt += " *";
        }
        return;
    }
    if (!t.isPrimitive) return;
    txt += " ";
    if (t.isArray) {
        txt += "vector ";
        txt += t.rosType;
    } else {
        txt += t.rosType;
    }
    txt += " *";
}

static void generateTypeMap(RosType& t, ConstString& txt) {
    txt = "";
    generateTypeMap1(t,txt);
    if (txt.length()>0) {
        txt = txt.substr(1,txt.length());
    }
    if (!t.reply) return;
    txt += " ---";
    generateTypeMap1(*(t.reply),txt);    
}

void configure_search(RosTypeSearch& env, Searchable& p) {
    if (p.check("out")) {
        env.setTargetDirectory(p.find("out").toString().c_str());
    }
    if (p.check("web",Value(0)).asInt()!=0 || p.findGroup("web").size()==1) {
        env.allowWeb();
    }
    if (p.check("soft",Value(0)).asInt()!=0 || p.findGroup("soft").size()==1) {
        env.softFail();
    }
    env.lookForService(p.check("service"));
}

int generate_cpp(int argc, char *argv[]) {
    bool is_service = false;

    Property p;
    string fname;
    p.fromCommand(argc,argv);

    fname = argv[argc-1];

    if (fname.rfind(".")!=string::npos) {
        string ext = fname.substr(fname.rfind("."),fname.length());
        if (ext==".srv" || ext==".SRV") {
            is_service = true;
        }
    }
    if (is_service) {
        p.put("service",1);
    }
 
    RosTypeSearch env;
    RosType t;

    RosTypeCodeGenYarp gen;
    if (p.check("out")) {
        gen.setTargetDirectory(p.find("out").toString().c_str());
    }
    configure_search(env,p);

    if (t.read(fname.c_str(),env,gen)) {
        RosTypeCodeGenState state;
        t.emitType(gen,state);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc<=1) {
        show_usage();
        return 0;
    }
    if (std::string("help")==argv[1] || std::string("--help")==argv[1]) {
        show_usage();
        return 0;        
    }

    Property p;
    p.fromCommand(argc,argv);

    if (!(p.check("name")||p.check("cmd"))) {
        return generate_cpp(argc,argv);
    }
    if (!p.check("soft")) {
        p.put("soft",1);
    }
    if (!p.check("web")) {
        p.put("web",1);
    }

    bool has_cmd = p.check("cmd");
    bool verbose = p.check("verbose");

    RosTypeSearch env;
    configure_search(env,p);

    Network yarp;
    Port port;
    if (!has_cmd) {
        // Borrow an accidentally-available service type on ROS, in
        // order to avoid adding build dependencies for now.
        port.promiseType(Type::byNameOnWire("test_roscpp/TestStringString"));
        port.setRpcServer();
        if (!port.open(p.find("name").asString())) {
            return 1;
        }
    }
    while (true) {
        Bottle req;
        if (has_cmd) {
            req = p.findGroup("cmd").tail();
        } else {
            if (!port.read(req,true)) {
                continue;
            }
        }
        if (req.size()==1) {
            req.fromString(req.get(0).asString());
        }
        if (verbose) {
            printf("Request: %s\n", req.toString().c_str());
        }
        Bottle resp;
        ConstString tag = req.get(0).asString();
        string fname0 = req.get(1).asString().c_str();
        string fname = env.findFile(fname0.c_str());
        string txt = "";
        if (tag=="raw") {
            txt = env.readFile(fname.c_str());
            resp.addString(txt);
        } else if (tag=="twiddle") {
            RosTypeCodeGenYarp gen;
            RosType t;
            if (t.read(fname0.c_str(),env,gen)) {
                ConstString txt;
                generateTypeMap(t,txt);
                resp.addString(txt);
            } else {
                resp.addString("?");
            }
        } else {
            resp.addString("?");
        }
        if (!has_cmd) port.reply(resp);
        if (verbose||has_cmd) {
            printf("Response: %s\n", resp.toString().c_str());
        }
        if (has_cmd) break;
    }
    return 0;
}
