#ifndef CMDPARSE_HPP
#define CMDPARSE_HPP

#include <boost/asio.hpp>
#include <iostream>

#include "postgresqlstruct.h"
#include "cmdtypes.h"
#include "server.h"

#include "Game/userposition.hpp"
#include "Game/log.hpp"
#include "NetWork/tcpconnection.h"
#include "PlayerLogin/playerlogin.h"

#include "GameTask/gametask.h"
#include "TeamFriend/teamfriend.h"
#include "GameAttack/gameattack.h"
#include "GameInterchange/gameinterchange.h"
#include "OperationGoods/operationgoods.h"
#include "Game/cmdparse.h"
#include "GameChat/gamechat.h"


void CommandParse(TCPConnection::Pointer conn , void *reg)
{
//     tcp::socket *sk =  &(conn->socket());
    char *buf = (char*)reg;
    STR_PackHead *head = (STR_PackHead*)buf;
    hf_uint16 flag = head->Flag;
    hf_uint16 len = head->Len;

    Server *srv = Server::GetInstance();

    PlayerLogin* t_playerLogin = srv->GetPlayerLogin();
//    GameTask* t_task = srv->GetGameTask();
//    TeamFriend* t_teamFriend = srv->GetTeamFriend();
    GameAttack* t_gameAttack = srv->GetGameAttack();
    GameInterchange* t_gameInterchange = srv->GetGameInterchange();
    OperationGoods* t_operationGoods = srv->GetOperationGoods();
    GameChat* t_gameChat = srv->GetGameChat();

    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    SessionMgr::SessionMap::iterator it = smap->find(conn);
    if(it == smap->end()) //未登录用户名，只能登录或注册用户
    {
        if(flag != FLAG_PlayerLoginUserId && flag != FLAG_PlayerRegisterUserId)
            return;
    }
    else
    {
        //已经登录用户不能登录或注册用户，只能退出用户后再登录或注册用户
        if(flag == FLAG_PlayerLoginUserId || flag == FLAG_PlayerRegisterUserId)
            return;
        if(it->second.m_roleid == 0) //未登录角色，只能登录或注册或删除角色
        {
            if(flag != FLAG_PlayerLoginRole && flag != FLAG_PlayerRegisterRole && flag != FLAG_UserDeleteRole)
                return;
        }
        else //已经登录角色不能再登录或注册或删除角色，只能退出角色后再登录或注册或删除角色
        {
            if(flag == FLAG_PlayerLoginRole || flag == FLAG_PlayerRegisterRole || flag == FLAG_UserDeleteRole)
                return;
            hf_uint32 hp = (*smap)[conn].m_roleInfo.HP;
            if(hp == 0)
            {
                if(flag != FLAG_PlayerRevive)
                    return;
            }
        }
    }

    switch( flag )
    {
    //注册玩家帐号
    case FLAG_PlayerRegisterUserId:
    {
        STR_PlayerRegisterUserId* reg = (STR_PlayerRegisterUserId*)srv->malloc();
        memcpy(reg, buf+sizeof(STR_PackHead), len);
        srv->RunTask(boost::bind(&PlayerLogin::RegisterUserID, t_playerLogin, conn, reg));
        break;
    }
        //注册玩家角色
    case FLAG_PlayerRegisterRole:
    {
        STR_PlayerRegisterRole* reg =(STR_PlayerRegisterRole*)srv->malloc();
        memcpy(reg, buf+sizeof(STR_PackHead), len);
        srv->RunTask(boost::bind(&PlayerLogin::RegisterRole, t_playerLogin, conn, reg));
        break;
    }
        //删除角色
    case FLAG_UserDeleteRole:
    {
        STR_PlayerRole* reg = (STR_PlayerRole*)(buf + sizeof(STR_PackHead));
        srv->RunTask(boost::bind(&PlayerLogin::DeleteRole, t_playerLogin, conn, reg->Role));
        break;
    }
        //登陆帐号
    case FLAG_PlayerLoginUserId:
    {
        STR_PlayerLoginUserId* reg = (STR_PlayerLoginUserId*)srv->malloc();
        memcpy(reg, buf+sizeof(STR_PackHead), len);
        srv->RunTask(boost::bind(&PlayerLogin::LoginUserId, t_playerLogin, conn, reg));
        break;
    }
        //登陆角色
    case FLAG_PlayerLoginRole:
    {
        STR_PlayerRole* reg = (STR_PlayerRole*)(buf + sizeof(STR_PackHead));
        srv->RunTask(boost::bind(&PlayerLogin::LoginRole, t_playerLogin, conn, reg->Role));
        break;
    }

    case FLAG_PlayerRevive:          //玩家复活   test
    {
        STR_PlayerRelive* reg = (STR_PlayerRelive*)(buf + sizeof(STR_PackHead));
        srv->RunTask(boost::bind(&PlayerLogin::PlayerRelive, t_playerLogin, conn, reg->mode));
        break;
    }

    case FLAG_PlayerOffline:  //玩家下线，给其他玩家发送下线通知
    {
        Logger::GetLogger()->Info("User Position recv");

        void *buf2 = srv->malloc();
        STR_PackPlayerOffline *pos =  new (buf2)STR_PackPlayerOffline ();
        memcpy(pos,buf,sizeof(STR_PackHead) + len);
        srv->RunTask(boost::bind(&PlayerLogin::PlayerOffline, t_playerLogin, conn, pos));
        break;
    }
    case FLAG_PlayerPosition:   //玩家位置变动(test)
    {
        STR_PackPlayerPosition* reg = (STR_PackPlayerPosition*)srv->malloc();
        memcpy(reg, buf, len + sizeof(STR_PackHead));
//        printf("玩家方向：%f\n",reg->Direct);

        srv->RunTask(boost::bind(UserPosition::PlayerMove, conn, reg));
        break;
    }
    case FLAG_PlayerMove:    //玩家移动
    {
        Server::GetInstance()->GetCmdParse()->PushPlayerMove(conn, (STR_PlayerMove*)(buf + sizeof(STR_PackHead)));

//        STR_PlayerMove* t_move = (STR_PlayerMove*)srv->malloc();
//        memcpy(t_move, buf + sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(UserPosition::PlayerPositionMove, conn, t_move));
        break;
    }
    case FLAG_UserAskTask: //玩家请求接受任务数据
    {
        hf_uint32 taskID = ((STR_PackUserAskTask*)buf)->TaskID;
//        hf_uint32 roleid = (*smap)[conn].m_roleid;
        Server::GetInstance()->GetCmdParse()->PushAskTask(conn, taskID);

//        srv->RunTask(boost::bind(&GameTask::AskTask, t_task, conn, taskID));
        break;
    }
    case FLAG_QuitTask:   //玩家请求放弃任务
    {
        hf_uint32 taskID = ((STR_PackQuitTask*)buf)->TaskID;
//        hf_uint32 roleid = (*smap)[conn].m_roleid;
        Server::GetInstance()->GetCmdParse()->PushQuitTask(conn, taskID);

//        srv->RunTask(boost::bind(&GameTask::QuitTask, t_task, conn, taskID));
        break;
    }
    case FLAG_AskFinishTask:  //玩家请求完成任务
    {

        Server::GetInstance()->GetCmdParse()->PushAskFinishTask(conn, (STR_FinishTask*)(buf + sizeof(STR_PackHead)));

//        STR_FinishTask* finishTask = (STR_FinishTask*)srv->malloc();
//        memcpy(finishTask, buf + sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&GameTask::AskFinishTask, t_task, conn, finishTask));
        break;
    }
    case FLAG_AskTaskData:    //请求任务数据包
    {               
        AskTaskData* t_ask = (AskTaskData*)(buf + sizeof(STR_PackHead));
        Server::GetInstance()->GetCmdParse()->PushAskTaskData(conn, t_ask->TaskID,t_ask->Flag);


//        switch(t_ask->Flag)
//        {
//        case FLAG_StartTaskDlg:
//        {
//            srv->RunTask(boost::bind(&GameTask::StartTaskDlg, t_task, conn, taskID));
//            break;
//        }
//        case FLAG_FinishTaskDlg:
//        {
//            srv->RunTask(boost::bind(&GameTask::FinishTaskDlg, t_task, conn, taskID));
//            break;
//        }
//        case FLAG_TaskDescription:
//        {
//            srv->RunTask(boost::bind(&GameTask::TaskDescription, t_task, conn, taskID));
//            break;
//        }
//        case FLAG_TaskAim:
//        {
//            srv->RunTask(boost::bind(&GameTask::TaskAim, t_task, conn, taskID));
//            break;
//        }
//        case FLAG_TaskReward:
//        {
//            srv->RunTask(boost::bind(&GameTask::TaskReward, t_task, conn, taskID));
//            break;
//        }
//        default:
//            break;
//        }
        break;
    }
    case FLAG_AddFriend:     //添加好友
    {
        Server::GetInstance()->GetCmdParse()->PushAddFriend(conn, (STR_PackAddFriend*)buf);

//        STR_PackAddFriend* t_add =(STR_PackAddFriend*)srv->malloc();
//        memcpy(t_add, buf, len + sizeof(STR_PackHead));
//        srv->RunTask(boost::bind(&TeamFriend::addFriend, t_teamFriend, conn, t_add));
        break;

    }
    case FLAG_DeleteFriend:   //删除好友
    {     
        STR_DeleteFriend* t_deleteFriend = (STR_DeleteFriend*)(buf + sizeof(STR_PackHead));
        Server::GetInstance()->GetCmdParse()->PushDeleteFriend(conn, t_deleteFriend->RoleID);

//        hf_uint32 friendID = t_deleteFriend->RoleID;
//        srv->RunTask(boost::bind(&TeamFriend::deleteFriend, t_teamFriend, conn, friendID));
        break;
    }
    case FLAG_AddFriendReturn:  //添加好友客户端返回
    {
        Server::GetInstance()->GetCmdParse()->PushAddFriendReturn(conn, (STR_PackAddFriendReturn*)buf);

//        STR_PackAddFriendReturn* t_AddFriend =(STR_PackAddFriendReturn*)srv->malloc();
//        memcpy(t_AddFriend, buf, len + sizeof(STR_PackHead));
//        srv->RunTask(boost::bind(&TeamFriend::ReciveAddFriend, t_teamFriend, conn, t_AddFriend));
        break;
    }
    case FLAG_UserAttackAim:  //攻击目标
    {
        STR_PackUserAttackAim* t_attack =(STR_PackUserAttackAim*)srv->malloc();
        memcpy(t_attack, buf, sizeof(STR_PackUserAttackAim));
        srv->RunTask(boost::bind(&GameAttack::AttackAim, t_gameAttack, conn, t_attack));
        break;
    }
    case FLAG_UserAttackPoint:  //攻击点
    {
        STR_PackUserAttackPoint* t_attack =(STR_PackUserAttackPoint*)srv->malloc();
        memcpy(t_attack, buf+sizeof(STR_PackHead), sizeof(STR_PackUserAttackPoint));
        srv->RunTask(boost::bind(&GameAttack::AttackPoint, t_gameAttack, conn, t_attack));
        break;
    }
    case FLAG_PickGoods:      //捡物品
    {
        Server::GetInstance()->GetCmdParse()->PushPickGoods(conn, (STR_PickGoods*)(buf + sizeof(STR_PackHead)));

//        STR_PickGoods* t_pick =(STR_PickGoods*)srv->malloc();
//        memcpy(t_pick, buf+sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&OperationGoods::PickUpGoods, t_operationGoods, conn, len, t_pick));
        break;
    }
    case FLAG_RemoveGoods:  //丢弃物品
    {
        Server::GetInstance()->GetCmdParse()->PushRemoveGoods(conn, (STR_RemoveBagGoods*)(buf + sizeof(STR_PackHead)));

//        STR_RemoveBagGoods* t_remove = (STR_RemoveBagGoods*)srv->malloc();
//        memcpy(t_remove, buf+sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&OperationGoods::RemoveBagGoods, t_operationGoods, conn, t_remove));
        break;
    }
    case FLAG_MoveGoods:  //移动或分割物品
    {
        Server::GetInstance()->GetCmdParse()->PushMoveGoods(conn, (STR_MoveBagGoods*)(buf + sizeof(STR_PackHead)));

//        STR_MoveBagGoods* t_move = (STR_MoveBagGoods*)srv->malloc();
//        memcpy(t_move, buf+sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&OperationGoods::MoveBagGoods, t_operationGoods, conn, t_move));
        break;
    }
    case FLAG_BuyGoods:    //购买物品
    {
        Server::GetInstance()->GetCmdParse()->PushBuyGoods(conn, (STR_BuyGoods*)(buf + sizeof(STR_PackHead)));

//        STR_BuyGoods* t_buy = (STR_BuyGoods*)srv->malloc();
//        memcpy(t_buy, buf + sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&OperationGoods::BuyGoods, t_operationGoods, conn, t_buy));
        break;
    }
    case FLAG_SellGoods:   //出售物品
    {
        Server::GetInstance()->GetCmdParse()->PushSellGoods(conn, (STR_SellGoods*)(buf + sizeof(STR_PackHead)));

//        STR_SellGoods* t_sell = (STR_SellGoods*)srv->malloc();
//        memcpy(t_sell, buf + sizeof(STR_PackHead), len);
//        srv->RunTask(boost::bind(&OperationGoods::SellGoods, t_operationGoods, conn, t_sell));
        break;
    }
    case FLAG_ArrangeBagGoods:
    {
        srv->RunTask(boost::bind(&OperationGoods::ArrangeBagGoods, t_operationGoods, conn));
        break;
    }

    case FLAG_WearBodyEqu: //穿装备
    {
        Server::GetInstance()->GetCmdParse()->PushWearBodyEqu(conn, (STR_WearEqu*)(buf + sizeof(STR_PackHead)));
        break;
    }

    case FLAG_TakeOffEqu:  //脱装备
    {
        hf_uint32 equid = *(hf_uint32*)(buf + sizeof(STR_PackHead));
        Server::GetInstance()->GetCmdParse()->PushTakeOffBodyEqu(conn, equid);
        break;
    }

    case FLAG_Chat:   //聊天 test
    {
        STR_PackRecvChat* t_chat = (STR_PackRecvChat*)srv->malloc();
        memcpy(t_chat, buf, len +sizeof(STR_PackHead));
        srv->RunTask(boost::bind(&GameChat::Chat, t_gameChat, conn, t_chat));
        break;
    }
    case FLAG_OperRequest:
    {
        operationRequest* t_operReq = (operationRequest*)srv->malloc();
        memcpy(t_operReq,buf,sizeof(STR_PackHead)+len);
        switch(t_operReq->operType)
        {
            case FLAG_WantToChange:
            {
                srv->RunTask(boost::bind(&GameInterchange::operRequest, t_gameInterchange, conn,t_operReq));
                break;
            }
            default:
                break;
        }
        break;
    }

    case FLAG_OperResult:
    {
        operationRequestResult* t_operReq = (operationRequestResult*)srv->malloc();
        memcpy(t_operReq,buf, len + sizeof(STR_PackHead));
        switch(t_operReq->operType)
        {
            case FLAG_WantToChange:
            {
                srv->RunTask(boost::bind(&GameInterchange::operResponse, t_gameInterchange, conn,t_operReq));
                break;
            }
            default:
                break;
        }
        break;
    }
    case FLAG_InterchangeMoneyCount:
    {
        interchangeMoney* t_Money = (interchangeMoney*)srv->malloc();
        memcpy(t_Money,buf,sizeof(STR_PackHead) + len);
        srv->RunTask(boost::bind(&GameInterchange::operMoneyChanges, t_gameInterchange, conn, t_Money));
        break;
    }
    case FLAG_InterchangeGoods:
    {
        interchangeOperGoods* t_OperPro = (interchangeOperGoods*)srv->malloc();
        memcpy(t_OperPro,buf,sizeof(STR_PackHead) + len);
        srv->RunTask(boost::bind(&GameInterchange::operChanges, t_gameInterchange, conn, t_OperPro));
        break;
    }
    case  FLAG_InterchangeOperPro:
    {
        interchangeOperPro* t_OperPro = (interchangeOperPro*)srv->malloc();
        memcpy(t_OperPro,buf,sizeof(STR_PackHead) + len);
        switch(t_OperPro->operType)
        {
            case Interchange_Change:  //交易
            {
                srv->RunTask(boost::bind(&GameInterchange::operProCheckChange, t_gameInterchange, conn, t_OperPro));
                break;
            }
            case Interchange_Lock:  //锁定
            {
                srv->RunTask(boost::bind(&GameInterchange::operProLock, t_gameInterchange, conn, t_OperPro));
                break;
            }
            case Interchange_Unlock:  //取消锁定
            {
                srv->RunTask(boost::bind(&GameInterchange::operProUnlock, t_gameInterchange, conn, t_OperPro));
                break;
            }
            case Interchange_CancelChange:  //取消交易
            {
                srv->RunTask(boost::bind(&GameInterchange::operProCancelChange, t_gameInterchange, conn, t_OperPro));
                break;
            }
            default:
                break;
        }
        break;
    }

    default:
        Logger::GetLogger()->Info("Unkown pack");
        break;
    }
}


#endif // CMDPARSE_HPP
