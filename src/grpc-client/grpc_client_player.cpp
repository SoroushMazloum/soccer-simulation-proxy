#include "grpc_client_player.h"
#include "state_generator.h"
#include "player/basic_actions/body_go_to_point.h"
#include "player/basic_actions/body_smart_kick.h"
#include "player/basic_actions/bhv_before_kick_off.h"
#include "player/basic_actions/bhv_body_neck_to_ball.h"
#include "player/basic_actions/bhv_body_neck_to_point.h"
#include "player/basic_actions/bhv_emergency.h"
#include "player/basic_actions/bhv_go_to_point_look_ball.h"
#include "player/basic_actions/bhv_neck_body_to_ball.h"
#include "player/basic_actions/bhv_neck_body_to_point.h"
#include "player/basic_actions/bhv_scan_field.h"
#include "player/basic_actions/body_advance_ball.h"
#include "player/basic_actions/body_clear_ball.h"
#include "player/basic_actions/body_dribble.h"
#include "player/basic_actions/body_go_to_point_dodge.h"
#include "player/basic_actions/body_hold_ball.h"
#include "player/basic_actions/body_intercept.h"
#include "player/basic_actions/body_kick_one_step.h"
#include "player/basic_actions/body_stop_ball.h"
#include "player/basic_actions/body_stop_dash.h"
#include "player/basic_actions/body_tackle_to_point.h"
#include "player/basic_actions/body_turn_to_angle.h"
#include "player/basic_actions/body_turn_to_point.h"
#include "player/basic_actions/body_turn_to_ball.h"
#include "player/basic_actions/focus_move_to_point.h"
#include "player/basic_actions/focus_reset.h"
#include "player/basic_actions/neck_scan_field.h"
#include "player/basic_actions/neck_scan_players.h"
#include "player/basic_actions/neck_turn_to_ball_and_player.h"
#include "player/basic_actions/neck_turn_to_ball_or_scan.h"
#include "player/basic_actions/neck_turn_to_ball.h"
#include "player/basic_actions/neck_turn_to_goalie_or_scan.h"
#include "player/basic_actions/neck_turn_to_low_conf_teammate.h"
#include "player/basic_actions/neck_turn_to_player_or_scan.h"
#include "player/basic_actions/neck_turn_to_point.h"
#include "player/basic_actions/neck_turn_to_relative.h"
#include "player/basic_actions/view_change_width.h"
#include "player/basic_actions/view_normal.h"
#include "player/basic_actions/view_wide.h"
#include "player/basic_actions/view_synch.h"
#include "player/role_goalie.h"
#include "planner/bhv_strict_check_shoot.h"
#include "player/bhv_basic_move.h"
#include "player/setplay/bhv_set_play.h"
#include "player/bhv_penalty_kick.h"
#include "planner/action_generator.h"
#include "planner/field_evaluator.h"
#include "player/sample_field_evaluator.h"
#include "planner/actgen_cross.h"
#include "planner/actgen_direct_pass.h"
#include "planner/actgen_self_pass.h"
#include "planner/actgen_strict_check_pass.h"
#include "planner/actgen_short_dribble.h"
#include "planner/actgen_simple_dribble.h"
#include "planner/actgen_shoot.h"
#include "planner/actgen_action_chain_length_filter.h"
#include "planner/action_chain_holder.h"
#include "planner/bhv_planned_action.h"
#include "player/strategy.h"
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/player_param.h>

#include <chrono>
#include <rcsc/common/logger.h>
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

// #define DEBUG_CLIENT_PLAYER

#ifdef DEBUG_CLIENT_PLAYER
#define LOG(x) std::cout << x << std::endl
#define LOGV(x) std::cout << #x << ": " << x << std::endl
#else
#define LOG(x)
#define LOGV(x)
#endif

GrpcClientPlayer::GrpcClientPlayer()
{
    M_agent_type = protos::AgentType::PlayerT;
}

void GrpcClientPlayer::init(rcsc::SoccerAgent *agent,
                            std::string target,
                            int port,
                            bool use_same_grpc_port,
                            bool add_20_to_grpc_port_if_right_side)
{
    M_agent = static_cast<rcsc::PlayerAgent *>(agent);
    M_unum = M_agent->world().self().unum();
    M_team_name = M_agent->world().ourTeamName();
    if (add_20_to_grpc_port_if_right_side)
        if (M_agent->world().ourSide() == rcsc::SideID::RIGHT)
            port += 20;

    if (!use_same_grpc_port)
    {
        port += M_agent->world().self().unum();
    }

    this->M_target = target + ":" + std::to_string(port);
    sample_communication = Communication::Ptr(new SampleCommunication());
}

void GrpcClientPlayer::getActions()
{
    auto agent = M_agent;
    bool pre_process = checkPreprocess(agent);
    bool do_forceKick = checkdoForceKick(agent);
    bool do_heardPassReceive = checkdoHeardPassReceive(agent);
    State state = generateState();
    state.set_need_preprocess(pre_process);
    protos::RegisterResponse* response = new protos::RegisterResponse(*M_register_response);
    state.set_allocated_register_response(response);
    protos::PlayerActions actions;
    ClientContext context;
    Status status = M_stub_->GetPlayerActions(&context, state, &actions);

    if (!status.ok())
    {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return;
    }

    if (pre_process && !actions.ignore_preprocess())
    {
        if (doPreprocess(agent))
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                      __FILE__": preprocess done" );
            return;
        }
    }

    if (do_forceKick && !actions.ignore_doforcekick())
    {
        if (doForceKick(agent))
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                      __FILE__": doForceKick done" );
            return;
        }
    }

    if (do_heardPassReceive && !actions.ignore_doheardpassrecieve())
    {
        if (doHeardPassReceive(agent))
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                      __FILE__": doHeardPassReceive done" );
            return;
        }
    }

    for (int i = 0; i < actions.actions_size(); i++)
    {
        auto action = actions.actions(i);
        if (action.action_case() == PlayerAction::kDash) {
            agent->doDash(action.dash().power(), action.dash().relative_direction());
        }
        else if (action.action_case() == PlayerAction::kKick) {
            agent->doKick(action.kick().power(), action.kick().relative_direction());
        }
        else if (action.action_case() == PlayerAction::kTurn) {
            agent->doTurn(action.turn().relative_direction());
        }
        else if (action.action_case() == PlayerAction::kTackle) {
            agent->doTackle(action.tackle().power_or_dir(), action.tackle().foul());
        }
        else if (action.action_case() == PlayerAction::kCatch) {
            agent->doCatch();
        }
        else if (action.action_case() == PlayerAction::kMove) {
            agent->doMove(action.move().x(), action.move().y());
        }
        else if (action.action_case() == PlayerAction::kTurnNeck) {
            agent->doTurnNeck(action.turn_neck().moment());
        }
        else if (action.action_case() == PlayerAction::kChangeView) {
            const rcsc::ViewWidth view_width = GrpcClient::convertViewWidth(action.change_view().view_width());
            agent->doChangeView(view_width);
        }
        else if (action.action_case() == PlayerAction::kSay) {
            addSayMessage(action.say());
        }
        else if (action.action_case() == PlayerAction::kPointTo) {
            agent->doPointto(action.point_to().x(), action.point_to().y());
        }
        else if (action.action_case() == PlayerAction::kPointToOf) {

            agent->doPointtoOff();
        }
        else if (action.action_case() == PlayerAction::kAttentionTo) {
            const rcsc::SideID side = GrpcClient::convertSideID(action.attention_to().side());
            agent->doAttentionto(side, action.attention_to().unum());
        }
        else if (action.action_case() == PlayerAction::kAttentionToOf) {
            agent->doAttentiontoOff();
        }
        else if (action.action_case() == PlayerAction::kLog) {
            addDlog(action.log());
        }
        else if (action.action_case() == PlayerAction::kBodyGoToPoint) {
            const auto &bodyGoToPoint = action.body_go_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyGoToPoint.target_point());
            Body_GoToPoint(targetPoint, bodyGoToPoint.distance_threshold(), bodyGoToPoint.max_dash_power()).execute(agent);
            agent->debugClient().addMessage("Body_GoToPoint");
        }
        else if (action.action_case() == PlayerAction::kBodySmartKick) {
            const auto &bodySmartKick = action.body_smart_kick();
            const auto &targetPoint = GrpcClient::convertVector2D(bodySmartKick.target_point());
            Body_SmartKick(targetPoint, bodySmartKick.first_speed(), bodySmartKick.first_speed_threshold(), bodySmartKick.max_steps()).execute(agent);
            agent->debugClient().addMessage("Body_SmartKick");
        }
        else if (action.action_case() == PlayerAction::kBhvBeforeKickOff) {
            const auto &bhvBeforeKickOff = action.bhv_before_kick_off();
            const auto &point = GrpcClient::convertVector2D(bhvBeforeKickOff.point());
            Bhv_BeforeKickOff(point).execute(agent);
                agent->debugClient().addMessage("Bhv_BeforeKickOff");
        }
        else if (action.action_case() == PlayerAction::kBhvBodyNeckToBall) {
            Bhv_BodyNeckToBall().execute(agent);
            agent->debugClient().addMessage("Bhv_BodyNeckToBall");
        }
        else if (action.action_case() == PlayerAction::kBhvBodyNeckToPoint) {
            const auto &bhvBodyNeckToPoint = action.bhv_body_neck_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(bhvBodyNeckToPoint.point());
            Bhv_BodyNeckToPoint(targetPoint).execute(agent);
            agent->debugClient().addMessage("Bhv_BodyNeckToPoint");
        }
        else if (action.action_case() == PlayerAction::kBhvEmergency) {
            Bhv_Emergency().execute(agent);
            agent->debugClient().addMessage("Bhv_Emergency");

        }
        else if (action.action_case() == PlayerAction::kBhvGoToPointLookBall) {
            const auto &bhvGoToPointLookBall = action.bhv_go_to_point_look_ball();
            const auto &targetPoint = GrpcClient::convertVector2D(bhvGoToPointLookBall.target_point());
            Bhv_GoToPointLookBall(targetPoint, bhvGoToPointLookBall.distance_threshold(), bhvGoToPointLookBall.max_dash_power()).execute(agent);
                agent->debugClient().addMessage("Bhv_GoToPointLookBall");
        }
        else if (action.action_case() == PlayerAction::kBhvNeckBodyToBall) {
            
            const auto &bhvNeckBodyToBall = action.bhv_neck_body_to_ball();
            Bhv_NeckBodyToBall(bhvNeckBodyToBall.angle_buf()).execute(agent);
            agent->debugClient().addMessage("Bhv_NeckBodyToBall");
        }
        else if (action.action_case() == PlayerAction::kBhvNeckBodyToPoint) {
            const auto &bhvNeckBodyToPoint = action.bhv_neck_body_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(bhvNeckBodyToPoint.point());
            Bhv_NeckBodyToPoint(targetPoint, bhvNeckBodyToPoint.angle_buf()).execute(agent);
            agent->debugClient().addMessage("Bhv_NeckBodyToPoint");
        }
        else if (action.action_case() == PlayerAction::kBhvScanField) {
            Bhv_ScanField().execute(agent);
            agent->debugClient().addMessage("Bhv_ScanField");
        }
        else if (action.action_case() == PlayerAction::kBodyAdvanceBall) {
            Body_AdvanceBall().execute(agent);
            agent->debugClient().addMessage("Body_AdvanceBall");
        }
        else if (action.action_case() == PlayerAction::kBodyClearBall) {
            Body_ClearBall().execute(agent);
            agent->debugClient().addMessage("Body_ClearBall");
        }
        else if (action.action_case() == PlayerAction::kBodyDribble) {
            const auto &bodyDribble = action.body_dribble();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyDribble.target_point());
            Body_Dribble(
                targetPoint,
                bodyDribble.distance_threshold(),
                bodyDribble.dash_power(),
                bodyDribble.dash_count(),
                bodyDribble.dodge())
                .execute(agent);
            agent->debugClient().addMessage("Body_Dribble");
        }
        else if (action.action_case() == PlayerAction::kBodyGoToPointDodge) {
            const auto &bodyGoToPointDodge = action.body_go_to_point_dodge();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyGoToPointDodge.target_point());
            Body_GoToPointDodge(
                targetPoint,
                bodyGoToPointDodge.dash_power())
                .execute(agent);
            agent->debugClient().addMessage("Body_GoToPointDodge");
        }
        else if (action.action_case() == PlayerAction::kBodyHoldBall) {
            const auto &bodyHoldBall = action.body_hold_ball();
            const auto &turnTargetPoint = GrpcClient::convertVector2D(bodyHoldBall.turn_target_point());
            const auto &kickTargetPoint = GrpcClient::convertVector2D(bodyHoldBall.kick_target_point());
            Body_HoldBall(
                bodyHoldBall.do_turn(),
                turnTargetPoint,
                kickTargetPoint)
                .execute(agent);
            agent->debugClient().addMessage("Body_HoldBall");
        }
        else if (action.action_case() == PlayerAction::kBodyIntercept) {
            const auto &bodyIntercept = action.body_intercept();
            const auto &facePoint = GrpcClient::convertVector2D(bodyIntercept.face_point());
            Body_Intercept(
                bodyIntercept.save_recovery(),
                facePoint)
                .execute(agent);
            agent->debugClient().addMessage("Body_Intercept");
        }
        else if (action.action_case() == PlayerAction::kBodyKickOneStep) {
            const auto &bodyKickOneStep = action.body_kick_one_step();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyKickOneStep.target_point());
            Body_KickOneStep(
                targetPoint,
                bodyKickOneStep.first_speed(),
                bodyKickOneStep.force_mode())
                .execute(agent);
            agent->debugClient().addMessage("Body_KickOneStep");
        }
        else if (action.action_case() == PlayerAction::kBodyStopBall) {
            Body_StopBall().execute(agent);
            agent->debugClient().addMessage("Body_StopBall");
        }
        else if (action.action_case() == PlayerAction::kBodyStopDash) {
            const auto &bodyStopDash = action.body_stop_dash();
            Body_StopDash(
                bodyStopDash.save_recovery())
                .execute(agent);
            agent->debugClient().addMessage("Body_StopDash");
        }
        else if (action.action_case() == PlayerAction::kBodyTackleToPoint) {
            const auto &bodyTackleToPoint = action.body_tackle_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyTackleToPoint.target_point());
            Body_TackleToPoint(
                targetPoint,
                bodyTackleToPoint.min_probability(),
                bodyTackleToPoint.min_speed())
                .execute(agent);
            agent->debugClient().addMessage("Body_TackleToPoint");
        }
        else if (action.action_case() == PlayerAction::kBodyTurnToAngle) {
            const auto &bodyTurnToAngle = action.body_turn_to_angle();
            Body_TurnToAngle(
                bodyTurnToAngle.angle())
                .execute(agent);
            agent->debugClient().addMessage("Body_TurnToAngle");
        }
        else if (action.action_case() == PlayerAction::kBodyTurnToBall) {
            const auto &bodyTurnToBall = action.body_turn_to_ball();
            Body_TurnToBall(
                bodyTurnToBall.cycle())
                .execute(agent);
            agent->debugClient().addMessage("Body_TurnToBall");
        }
        else if (action.action_case() == PlayerAction::kBodyTurnToPoint) {
            const auto &bodyTurnToPoint = action.body_turn_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(bodyTurnToPoint.target_point());
            Body_TurnToPoint(
                targetPoint,
                bodyTurnToPoint.cycle())
                .execute(agent);
            agent->debugClient().addMessage("Body_TurnToPoint");
        }
        else if (action.action_case() == PlayerAction::kFocusMoveToPoint) {
            const auto &focusMoveToPoint = action.focus_move_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(focusMoveToPoint.target_point());
            rcsc::Focus_MoveToPoint(
                targetPoint)
                .execute(agent);
            agent->debugClient().addMessage("Focus_MoveToPoint");
        }
        else if (action.action_case() == PlayerAction::kFocusReset) {
            rcsc::Focus_Reset().execute(agent);
            agent->debugClient().addMessage("Focus_Reset");
        }
        else if (action.action_case() == PlayerAction::kNeckScanField) {
            Neck_ScanField().execute(agent);
            agent->debugClient().addMessage("Neck_ScanField");
        }
        else if (action.action_case() == PlayerAction::kNeckScanPlayers) {
            Neck_ScanPlayers().execute(agent);
            agent->debugClient().addMessage("Neck_ScanPlayers");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToBallAndPlayer) {
            const auto &neckTurnToBallAndPlayer = action.neck_turn_to_ball_and_player();
            const rcsc::AbstractPlayerObject *player = nullptr;
            if (neckTurnToBallAndPlayer.side() == protos::Side::LEFT && agent->world().ourSide() == rcsc::SideID::LEFT){
                player = agent->world().ourPlayer(neckTurnToBallAndPlayer.uniform_number());
            }
            else{
                player = agent->world().theirPlayer(neckTurnToBallAndPlayer.uniform_number());
            }
            if (player != nullptr){
                Neck_TurnToBallAndPlayer(
                    player,
                    neckTurnToBallAndPlayer.count_threshold())
                    .execute(agent);
                agent->debugClient().addMessage("Neck_TurnToBallAndPlayer");
            }
            else 
            {
                agent->debugClient().addMessage("Neck_TurnToBallAndPlayer null player");
            }
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToBallOrScan) {
            const auto &neckTurnToBallOrScan = action.neck_turn_to_ball_or_scan();
            Neck_TurnToBallOrScan(
                neckTurnToBallOrScan.count_threshold())
                .execute(agent);
            agent->debugClient().addMessage("Neck_TurnToBallOrScan");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToBall) {
            Neck_TurnToBall().execute(agent);
            agent->debugClient().addMessage("Neck_TurnToBall");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToGoalieOrScan) {
            const auto &neckTurnToGoalieOrScan = action.neck_turn_to_goalie_or_scan();
            Neck_TurnToGoalieOrScan(
                neckTurnToGoalieOrScan.count_threshold())
                .execute(agent);
            agent->debugClient().addMessage("Neck_TurnToGoalieOrScan");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToLowConfTeammate) {
            const auto &neckTurnToLowConfTeammate = action.neck_turn_to_low_conf_teammate();
            Neck_TurnToLowConfTeammate().execute(agent);
            agent->debugClient().addMessage("Neck_TurnToLowConfTeammate");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToPlayerOrScan) {
            const auto &neckTurnToPlayerOrScan = action.neck_turn_to_player_or_scan();
            const rcsc::AbstractPlayerObject *player = nullptr;
            if (neckTurnToPlayerOrScan.side() == protos::Side::LEFT && agent->world().ourSide() == rcsc::SideID::LEFT){
                player = agent->world().ourPlayer(neckTurnToPlayerOrScan.uniform_number());
            }
            else{
                player = agent->world().theirPlayer(neckTurnToPlayerOrScan.uniform_number());
            }
            if (player != nullptr){
                Neck_TurnToPlayerOrScan(
                    player,
                    neckTurnToPlayerOrScan.count_threshold())
                    .execute(agent);
                agent->debugClient().addMessage("Neck_TurnToPlayerOrScan");
            }
            else 
            {
                agent->debugClient().addMessage("Neck_TurnToPlayerOrScan null player");
            }
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToPoint) {
            const auto &neckTurnToPoint = action.neck_turn_to_point();
            const auto &targetPoint = GrpcClient::convertVector2D(neckTurnToPoint.target_point());
            Neck_TurnToPoint(
                targetPoint)
                .execute(agent);
            agent->debugClient().addMessage("Neck_TurnToPoint");
        }
        else if (action.action_case() == PlayerAction::kNeckTurnToRelative) {
            const auto &neckTurnToRelative = action.neck_turn_to_relative();
            Neck_TurnToRelative(
                neckTurnToRelative.angle())
                .execute(agent);
            agent->debugClient().addMessage("Neck_TurnToRelative");
        }
        else if (action.action_case() == PlayerAction::kViewChangeWidth) {
            const auto &viewChangeWidth = action.view_change_width();
            const rcsc::ViewWidth view_width = GrpcClient::convertViewWidth(viewChangeWidth.view_width());
            View_ChangeWidth(
                view_width)
                .execute(agent);
            agent->debugClient().addMessage("View_ChangeWidth");
        }
        else if (action.action_case() == PlayerAction::kViewNormal) {
            View_Normal().execute(agent);
            agent->debugClient().addMessage("View_Normal");
        }
        else if (action.action_case() == PlayerAction::kViewWide) {
            View_Wide().execute(agent);
            agent->debugClient().addMessage("View_Wide");
        }
        else if (action.action_case() == PlayerAction::kViewSynch) {
            View_Synch().execute(agent);
            agent->debugClient().addMessage("View_Synch");
        }
        else if (action.action_case() == PlayerAction::kHeliosGoalie) {
            RoleGoalie roleGoalie = RoleGoalie();
            roleGoalie.execute(agent);
            agent->debugClient().addMessage("RoleGoalie - execute");
        }
        else if (action.action_case() == PlayerAction::kHeliosGoalieMove) {
            RoleGoalie roleGoalie = RoleGoalie();
            roleGoalie.doMove(agent);
            agent->debugClient().addMessage("RoleGoalie - do Move");
        }
        else if (action.action_case() == PlayerAction::kHeliosGoalieKick) {
            RoleGoalie roleGoalie = RoleGoalie();
            roleGoalie.doKick(agent);
            agent->debugClient().addMessage("RoleGoalie - do Kick");
        }
        else if (action.action_case() == PlayerAction::kHeliosShoot) {
            const rcsc::WorldModel &wm = agent->world();
            
            if (wm.gameMode().type() != rcsc::GameMode::IndFreeKick_ && wm.time().stopped() == 0 && wm.self().isKickable() && Bhv_StrictCheckShoot().execute(agent))
                {
            }
        }
        else if (action.action_case() == PlayerAction::kHeliosBasicMove) {
            Bhv_BasicMove().execute(agent);
            agent->debugClient().addMessage("Bhv_BasicMove");
        }
        else if (action.action_case() == PlayerAction::kHeliosSetPlay) {
            Bhv_SetPlay().execute(agent);
            agent->debugClient().addMessage("Bhv_SetPlay");
        }
        else if (action.action_case() == PlayerAction::kHeliosPenalty) {
            Bhv_PenaltyKick().execute(agent);
            agent->debugClient().addMessage("Bhv_PenaltyKick");
        }
        else if (action.action_case() == PlayerAction::kHeliosCommunication) {
                sample_communication->execute(agent);
                agent->debugClient().addMessage("sample_communication - execute");
        }
        else if (action.action_case() == PlayerAction::kBhvDoForceKick)
        {
            if(doForceKick(agent))
            {
                agent->debugClient().addMessage("doForceKick");
            }
            else
            {
                agent->debugClient().addMessage("doForceKick - false");
            }
        }
        else if (action.action_case() == PlayerAction::kBhvDoHeardPassRecieve)
        {
            if(doHeardPassReceive(agent))
            {
                agent->debugClient().addMessage("doHeardPassReceive");
            }
            else
            {
                agent->debugClient().addMessage("doHeardPassReceive - false");
            }
        }
        else if (action.action_case() == PlayerAction::kHeliosOffensivePlanner) {
            FieldEvaluator::ConstPtr field_evaluator = FieldEvaluator::ConstPtr(new SampleFieldEvaluator);
            CompositeActionGenerator *g = new CompositeActionGenerator();
            
            if (action.helios_offensive_planner().lead_pass() 
                || action.helios_offensive_planner().direct_pass() || action.helios_offensive_planner().through_pass())
                g->addGenerator(new ActGen_MaxActionChainLengthFilter(new ActGen_StrictCheckPass(), 1));
            if (action.helios_offensive_planner().cross())
                g->addGenerator(new ActGen_MaxActionChainLengthFilter(new ActGen_Cross(), 1));
            if (action.helios_offensive_planner().simple_pass())
                g->addGenerator(new ActGen_RangeActionChainLengthFilter(new ActGen_DirectPass(),
                                                                        2, ActGen_RangeActionChainLengthFilter::MAX));
            if (action.helios_offensive_planner().short_dribble())
                g->addGenerator(new ActGen_MaxActionChainLengthFilter(new ActGen_ShortDribble(), 1));
            if (action.helios_offensive_planner().long_dribble())
                g->addGenerator(new ActGen_MaxActionChainLengthFilter(new ActGen_SelfPass(), 1));
            if (action.helios_offensive_planner().simple_dribble())
                g->addGenerator(new ActGen_RangeActionChainLengthFilter(new ActGen_SimpleDribble(),
                                                                        2, ActGen_RangeActionChainLengthFilter::MAX));
            if (action.helios_offensive_planner().simple_shoot())
                g->addGenerator(new ActGen_RangeActionChainLengthFilter(new ActGen_Shoot(),
                                                                        2, ActGen_RangeActionChainLengthFilter::MAX));
            if (g->M_generators.empty())
            {
                Body_HoldBall().execute(agent);
                agent->setNeckAction(new Neck_ScanField());
                continue;
            }
            ActionGenerator::ConstPtr action_generator = ActionGenerator::ConstPtr(g);
            ActionChainHolder::instance().setFieldEvaluator(field_evaluator);
            ActionChainHolder::instance().setActionGenerator(action_generator);
            ActionChainHolder::instance().update(agent->world());
            
            if (action.helios_offensive_planner().server_side_decision())
            {
                if (GetBestPlannerAction())
                {
                    agent->debugClient().addMessage("GetBestPlannerAction");
                }
            }
            else
            {
                if (Bhv_PlannedAction().execute(agent))
                {
                    agent->debugClient().addMessage("PlannedAction");
                }
                
            }

        }
        else 
        {
            #ifdef DEBUG_CLIENT_PLAYER
            std::cout << "Unkown action"<<std::endl;
            #endif
            Body_HoldBall().execute(agent);
            agent->setNeckAction(new Neck_ScanField());
        }
    }
}

bool GrpcClientPlayer::GetBestPlannerAction()
{
    protos::BestPlannerActionRequest *action_state_pairs = new protos::BestPlannerActionRequest();
    protos::RegisterResponse* response = new protos::RegisterResponse(*M_register_response);
    action_state_pairs->set_allocated_register_response(response);
    State state = generateState();
    state.set_allocated_register_response(response);
    action_state_pairs->set_allocated_state(&state);
    #ifdef DEBUG_CLIENT_PLAYER
    std::cout << "GetBestActionStatePair:" << "c" << M_agent->world().time().cycle() << std::endl;
    std::cout << "results size:" << ActionChainHolder::instance().graph().getAllResults().size() << std::endl;
    #endif
    auto map = action_state_pairs->mutable_pairs();
    convertResultPairToRpcActionStatePair(map);
    #ifdef DEBUG_CLIENT_PLAYER
    std::cout << "map size:" << action_state_pairs->pairs_size() << std::endl;
    #endif
    protos::BestPlannerActionResponse best_action;
    ClientContext context;
    Status status = M_stub_->GetBestPlannerAction(&context, *action_state_pairs, &best_action);

    if (!status.ok())
    {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return false;
    }

    auto agent = M_agent;

    #ifdef DEBUG_CLIENT_PLAYER
    std::cout << "best action index:" << best_action.index() << std::endl;
    #endif

    if (Bhv_PlannedAction().execute(agent, best_action.index()))
    {
        #ifdef DEBUG_CLIENT_PLAYER
        std::cout << "PlannedAction" << std::endl;
        #endif
        agent->debugClient().addMessage("PlannedAction");
        return true;
    }

    return false;
}

void GrpcClientPlayer::addSayMessage(protos::Say sayMessage) const
{
    auto agent = M_agent;
    switch (sayMessage.message_case())
    {
    case protos::Say::kBallMessage:
    {
        const auto &ballMessage = sayMessage.ball_message();
        const auto &ballPosition = GrpcClient::convertVector2D(ballMessage.ball_position());
        const auto &ballVelocity = GrpcClient::convertVector2D(ballMessage.ball_velocity());
        agent->addSayMessage(new rcsc::BallMessage(ballPosition, ballVelocity));
        break;
    }
    case protos::Say::kPassMessage:
    {
        const auto &passMessage = sayMessage.pass_message();
        const auto &receiverPoint = GrpcClient::convertVector2D(passMessage.receiver_point());
        const auto &ballPosition = GrpcClient::convertVector2D(passMessage.ball_position());
        const auto &ballVelocity = GrpcClient::convertVector2D(passMessage.ball_velocity());
        agent->addSayMessage(new rcsc::PassMessage(passMessage.receiver_uniform_number(),
                                                   receiverPoint,
                                                   ballPosition,
                                                   ballVelocity));
        break;
    }
    case protos::Say::kInterceptMessage:
    {
        const auto &interceptMessage = sayMessage.intercept_message();
        agent->addSayMessage(new rcsc::InterceptMessage(interceptMessage.our(),
                                                        interceptMessage.uniform_number(),
                                                        interceptMessage.cycle()));
        break;
    }
    case protos::Say::kGoalieMessage:
    {
        const auto &goalieMessage = sayMessage.goalie_message();
        const auto &goaliePosition = GrpcClient::convertVector2D(goalieMessage.goalie_position());
        agent->addSayMessage(new rcsc::GoalieMessage(goalieMessage.goalie_uniform_number(),
                                                     goaliePosition,
                                                     goalieMessage.goalie_body_direction()));
        break;
    }
    case protos::Say::kGoalieAndPlayerMessage:
    {
        const auto &goalieAndPlayerMessage = sayMessage.goalie_and_player_message();
        const auto &goaliePosition = GrpcClient::convertVector2D(goalieAndPlayerMessage.goalie_position());
        const auto &playerPosition = GrpcClient::convertVector2D(goalieAndPlayerMessage.player_position());
        agent->addSayMessage(new rcsc::GoalieAndPlayerMessage(goalieAndPlayerMessage.goalie_uniform_number(),
                                                              goaliePosition,
                                                              goalieAndPlayerMessage.goalie_body_direction(),
                                                              goalieAndPlayerMessage.player_uniform_number(),
                                                              playerPosition));
        break;
    }
    case protos::Say::kOffsideLineMessage:
    {
        const auto &offsideLineMessage = sayMessage.offside_line_message();
        agent->addSayMessage(new rcsc::OffsideLineMessage(offsideLineMessage.offside_line_x()));
        break;
    }
    case protos::Say::kDefenseLineMessage:
    {
        const auto &defenseLineMessage = sayMessage.defense_line_message();
        agent->addSayMessage(new rcsc::DefenseLineMessage(defenseLineMessage.defense_line_x()));
        break;
    }
    case protos::Say::kWaitRequestMessage:
    {
        const auto &waitRequestMessage = sayMessage.wait_request_message();
        agent->addSayMessage(new rcsc::WaitRequestMessage());
        break;
    }
    case protos::Say::kSetplayMessage:
    {
        const auto &setplayMessage = sayMessage.setplay_message();
        agent->addSayMessage(new rcsc::SetplayMessage(setplayMessage.wait_step()));
        break;
    }
    case protos::Say::kPassRequestMessage:
    {
        const auto &passRequestMessage = sayMessage.pass_request_message();
        const auto &targetPoint = GrpcClient::convertVector2D(passRequestMessage.target_point());
        agent->addSayMessage(new rcsc::PassRequestMessage(targetPoint));
        break;
    }
    case protos::Say::kStaminaMessage:
    {
        const auto &staminaMessage = sayMessage.stamina_message();
        agent->addSayMessage(new rcsc::StaminaMessage(staminaMessage.stamina()));
        break;
    }
    case protos::Say::kRecoveryMessage:
    {
        const auto &recoveryMessage = sayMessage.recovery_message();
        agent->addSayMessage(new rcsc::RecoveryMessage(recoveryMessage.recovery()));
        break;
    }
    case protos::Say::kStaminaCapacityMessage:
    {
        const auto &staminaCapacityMessage = sayMessage.stamina_capacity_message();
        agent->addSayMessage(new rcsc::StaminaCapacityMessage(staminaCapacityMessage.stamina_capacity()));
        break;
    }
    case protos::Say::kDribbleMessage:
    {
        const auto &dribbleMessage = sayMessage.dribble_message();
        const auto &targetPoint = GrpcClient::convertVector2D(dribbleMessage.target_point());
        agent->addSayMessage(new rcsc::DribbleMessage(targetPoint, dribbleMessage.queue_count()));
        break;
    }
    case protos::Say::kBallGoalieMessage:
    {
        const auto &ballGoalieMessage = sayMessage.ball_goalie_message();
        const auto &ballPosition = GrpcClient::convertVector2D(ballGoalieMessage.ball_position());
        const auto &ballVelocity = GrpcClient::convertVector2D(ballGoalieMessage.ball_velocity());
        const auto &goaliePosition = GrpcClient::convertVector2D(ballGoalieMessage.goalie_position());
        agent->addSayMessage(new rcsc::BallGoalieMessage(ballPosition, ballVelocity, goaliePosition, ballGoalieMessage.goalie_body_direction()));
        break;
    }
    case protos::Say::kOnePlayerMessage:
    {
        const auto &onePlayerMessage = sayMessage.one_player_message();
        const auto &playerPosition = GrpcClient::convertVector2D(onePlayerMessage.position());
        agent->addSayMessage(new rcsc::OnePlayerMessage(onePlayerMessage.uniform_number(), playerPosition));
        break;
    }
    case protos::Say::kTwoPlayerMessage:
    {
        const auto &twoPlayersMessage = sayMessage.two_player_message();
        const auto &player1Position = GrpcClient::convertVector2D(twoPlayersMessage.first_position());
        const auto &player2Position = GrpcClient::convertVector2D(twoPlayersMessage.second_position());
        agent->addSayMessage(new rcsc::TwoPlayerMessage(twoPlayersMessage.first_uniform_number(),
                                                        player1Position,
                                                        twoPlayersMessage.second_uniform_number(),
                                                        player2Position));
        break;
    }
    case protos::Say::kThreePlayerMessage:
    {
        const auto &threePlayersMessage = sayMessage.three_player_message();
        const auto &player1Position = GrpcClient::convertVector2D(threePlayersMessage.first_position());
        const auto &player2Position = GrpcClient::convertVector2D(threePlayersMessage.second_position());
        const auto &player3Position = GrpcClient::convertVector2D(threePlayersMessage.third_position());
        agent->addSayMessage(new rcsc::ThreePlayerMessage(threePlayersMessage.first_uniform_number(),
                                                          player1Position,
                                                          threePlayersMessage.second_uniform_number(),
                                                          player2Position,
                                                          threePlayersMessage.third_uniform_number(),
                                                          player3Position));
        break;
    }
    case protos::Say::kSelfMessage:
    {
        const auto &selfMessage = sayMessage.self_message();
        const auto &selfPosition = GrpcClient::convertVector2D(selfMessage.self_position());
        agent->addSayMessage(new rcsc::SelfMessage(selfPosition, selfMessage.self_body_direction(), selfMessage.self_stamina()));
        break;
    }
    case protos::Say::kTeammateMessage:
    {
        const auto &teammateMessage = sayMessage.teammate_message();
        const auto &teammatePosition = GrpcClient::convertVector2D(teammateMessage.position());
        agent->addSayMessage(new rcsc::TeammateMessage(teammateMessage.uniform_number(), teammatePosition, teammateMessage.body_direction()));
        break;
    }
    case protos::Say::kOpponentMessage:
    {
        const auto &opponentMessage = sayMessage.opponent_message();
        const auto &opponentPosition = GrpcClient::convertVector2D(opponentMessage.position());
        agent->addSayMessage(new rcsc::OpponentMessage(opponentMessage.uniform_number(), opponentPosition, opponentMessage.body_direction()));
        break;
    }
    case protos::Say::kBallPlayerMessage:
    {
        const auto &ballPlayerMessage = sayMessage.ball_player_message();
        const auto &ballPosition = GrpcClient::convertVector2D(ballPlayerMessage.ball_position());
        const auto &ballVelocity = GrpcClient::convertVector2D(ballPlayerMessage.ball_velocity());
        const auto &playerPosition = GrpcClient::convertVector2D(ballPlayerMessage.player_position());
        agent->addSayMessage(new rcsc::BallPlayerMessage(ballPosition, ballVelocity, ballPlayerMessage.uniform_number(), playerPosition, ballPlayerMessage.body_direction()));
        break;
    }
    default:
    {
        std::cout << "GrpcClient: unknown say message" << std::endl;
        break;
    }
    }
}

protos::State GrpcClientPlayer::generateState() 
{
    const rcsc::WorldModel &wm = M_agent->world();
    if (M_state_update_time == wm.time())
    {
        return M_state;
    }
    M_state_update_time = wm.time();
    WorldModel *worldModel = StateGenerator::convertWorldModel(wm);
    addHomePosition(worldModel);
    protos::State state;
    state.set_allocated_world_model(worldModel);
    M_state = state;
    return M_state;
}

void GrpcClientPlayer::addHomePosition(protos::WorldModel *res) const
{
    for (int i = 1; i < 12; i++)
    {
        auto map = res->mutable_helios_home_positions();
        auto home_pos = Strategy::i().getPosition(i);
        auto vec_msg = protos::RpcVector2D();
        vec_msg.set_x(home_pos.x);
        vec_msg.set_y(home_pos.y);
        (*map)[i] = vec_msg;
    }
}
void GrpcClientPlayer::convertResultPairToRpcActionStatePair( google::protobuf::Map<int32_t, protos::RpcActionState> * map)
{
    for (auto & index_resultPair : ActionChainHolder::instance().graph().getAllResults())
    {
        try
        {
            
            auto & result_pair = index_resultPair.second;
            auto action_ptr = result_pair.first->actionPtr();
            auto state_ptr = result_pair.first->statePtr();
            int unique_index = action_ptr->uniqueIndex();
            int parent_index = action_ptr->parentIndex();
            auto eval = result_pair.second;
            #ifdef DEBUG_CLIENT_PLAYER
            std::cout<<"index:"<<index_resultPair.first<<" "<<unique_index<<" "<<parent_index<<" "<<eval<<std::endl;
            #endif
            auto rpc_action_state_pair = protos::RpcActionState();
            auto rpc_cooperative_action = new protos::RpcCooperativeAction();
            auto rpc_predict_state = new protos::RpcPredictState();
            auto category = protos::RpcActionCategory::AC_Hold;
            
            switch (action_ptr->category())
            {
            case CooperativeAction::Hold:
                category = protos::RpcActionCategory::AC_Hold;
                break;
            case CooperativeAction::Dribble:
                category = protos::RpcActionCategory::AC_Dribble;
                break;
            case CooperativeAction::Pass:
                category = protos::RpcActionCategory::AC_Pass;
                break;
            case CooperativeAction::Shoot:
                category = protos::RpcActionCategory::AC_Shoot;
                break;
            case CooperativeAction::Clear:
                category = protos::RpcActionCategory::AC_Clear;
                break;
            case CooperativeAction::Move:
                category = protos::RpcActionCategory::AC_Move;
                break;
            case CooperativeAction::NoAction:
                category = protos::RpcActionCategory::AC_NoAction;
                break;
            default:
                break;
            }
            rpc_cooperative_action->set_category(category);
            rpc_cooperative_action->set_index(unique_index);
            rpc_cooperative_action->set_sender_unum(action_ptr->playerUnum());
            rpc_cooperative_action->set_target_unum(action_ptr->targetPlayerUnum());
            rpc_cooperative_action->set_allocated_target_point(StateGenerator::convertVector2D(action_ptr->targetPoint()));
            rpc_cooperative_action->set_first_ball_speed(action_ptr->firstBallSpeed());
            rpc_cooperative_action->set_first_turn_moment(action_ptr->firstTurnMoment());
            rpc_cooperative_action->set_first_dash_power(action_ptr->firstDashPower());
            rpc_cooperative_action->set_first_dash_angle_relative(action_ptr->firstDashAngle().degree());
            rpc_cooperative_action->set_duration_step(action_ptr->durationStep());
            rpc_cooperative_action->set_kick_count(action_ptr->kickCount());
            rpc_cooperative_action->set_turn_count(action_ptr->turnCount());
            rpc_cooperative_action->set_dash_count(action_ptr->dashCount());
            rpc_cooperative_action->set_final_action(action_ptr->isFinalAction());
            rpc_cooperative_action->set_description(action_ptr->description());
            rpc_cooperative_action->set_parent_index(parent_index);

            rpc_predict_state->set_spend_time(state_ptr->spendTime());
            rpc_predict_state->set_ball_holder_unum(state_ptr->ballHolderUnum());
            rpc_predict_state->set_allocated_ball_position(StateGenerator::convertVector2D(state_ptr->ball().pos()));
            rpc_predict_state->set_allocated_ball_velocity(StateGenerator::convertVector2D(state_ptr->ball().vel()));
            rpc_predict_state->set_our_defense_line_x(state_ptr->ourDefenseLineX());
            rpc_predict_state->set_our_offense_line_x(state_ptr->ourOffensePlayerLineX());

            rpc_action_state_pair.set_allocated_action(rpc_cooperative_action);
            rpc_action_state_pair.set_allocated_predict_state(rpc_predict_state);
            rpc_action_state_pair.set_evaluation(eval);

            (*map)[unique_index] = rpc_action_state_pair;
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << '\n';
        }
    }
}
