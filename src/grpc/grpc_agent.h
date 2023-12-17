#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"
#include "service.pb.h"
#include <rcsc/player/player_agent.h>


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using protos::Game;
using protos::State;
using protos::Action;

class GrpcAgent {
public:
    std::shared_ptr<Channel> channel;
    std::unique_ptr<Game::Stub> stub_;
    GrpcAgent() {}

    void init(std::string target="localhost:50051"){
        channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
        stub_ = Game::NewStub(channel);
    }

    ~GrpcAgent() {}
    void getAction(rcsc::PlayerAgent *agent) const;
};