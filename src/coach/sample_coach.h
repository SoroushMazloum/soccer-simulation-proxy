// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef SAMPLE_COACH_H
#define SAMPLE_COACH_H

#include <rcsc/coach/coach_agent.h>
#include <rcsc/types.h>
#ifdef USE_THRIFT
#include "thrift-client/thrift_client_coach.h"
#endif
#ifdef USE_GRPC
#include "grpc-client/grpc_client_coach.h"
#endif
#include "rpc-client/rpc-client.h"
#include "rpc-client/rpc-agent.h"
#include <vector>


namespace rcsc {
class PlayerType;
}


class SampleCoach
    : public rcsc::CoachAgent, public RpcAgent{
private:
    typedef std::vector< const rcsc::PlayerType * > PlayerTypePtrCont;


    int M_opponent_player_types[11];

    rcsc::TeamGraphic M_team_graphic;

public:

    SampleCoach();

    virtual
    ~SampleCoach();

    void SetRpcType(bool use_thrift){
        M_use_thrift = use_thrift;
        if (use_thrift){
#ifdef USE_THRIFT
            M_rpc_client = new ThriftClientCoach();
#endif
        }
        else
        {
#ifdef USE_GRPC
            M_rpc_client = new GrpcClientCoach();
#endif
        }
    }
    
    void doSubstitute();
    void sayPlayerTypes();

protected:

    /*!
      You can override this method.
      But, CoachAgent::initImpl() must be called in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision making
    virtual
    void actionImpl();
    void
    handleExit() override;


    virtual
    void handleInitMessage();
    virtual
    void handleServerParam();
    virtual
    void handlePlayerParam();
    virtual
    void handlePlayerType();

private:

    void doFirstSubstitute();
    void doSubstituteTiredPlayers();

    void substituteTo( const int unum,
                       const int type );

    int getFastestType( PlayerTypePtrCont & candidates );

    void sendTeamGraphic();
};

#endif
