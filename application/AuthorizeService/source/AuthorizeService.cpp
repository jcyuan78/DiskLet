#include "pch.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// 
// AuthorizeService.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <memory>
#include <cstdlib>
#include <restbed>

#include <iostream>

#include <LibAuthorGen.h>

#include "AuthorizeService.h"

#include <boost/property_tree/json_parser.hpp>
#include <strstream>



//#ifdef _DEBUG
//#include <vld.h>
//#endif

#define MAX_LINEBUF	1024


//namespace prop_tree = boost::property_tree;
//namespace qi = boost::spirit::qi;
//namespace ascii = boost::spirit::ascii;


LOCAL_LOGGER_ENABLE(L"servive", LOGGER_LEVEL_DEBUGINFO);

const TCHAR CAuthorizeServiceApp::LOG_CONFIG_FN[] = L"authorize.cfg";
typedef jcvos::CJCApp<CAuthorizeServiceApp>	CApplication;
CApplication _app;

#define _class_name_	CApplication
BEGIN_ARGU_DEF_TABLE()
ARGU_DEF(L"config", 'c', m_config_fn, L"config file in xml")
//ARGU_DEF(L"dst_size", 's', m_disk_size, L"destination size for test, in MB")
//ARGU_DEF(L"list", 'l', m_list_volume, L"list all volume", false)
//ARGU_DEF(L"dst", 'd', m_destination, L"destination volume")
//ARGU_DEF(L"copy_file", 'f', m_copy_file, L"copy by file", false)
//ARGU_DEF(L"partition", 'p', m_partition, L"copy by file")
//ARGU_DEF(L"tar_disk", 't', m_tar_disk, L"copy by file")

END_ARGU_DEF_TABLE()

int _tmain(int argc, _TCHAR* argv[])
{
	return jcvos::local_main(argc, argv);
}

CAuthorizeServiceApp::CAuthorizeServiceApp(void)
{
}

CAuthorizeServiceApp::~CAuthorizeServiceApp(void)
{
}

int CAuthorizeServiceApp::Initialize(void)
{
    return 0;
}

void CAuthorizeServiceApp::CleanUp(void)
{
}

void CAuthorizeServiceApp::LoadConfig(const std::wstring& config)
{
    m_app_info.LoadAppInfo(config);
}

void get_method_handler(const std::shared_ptr< restbed::Session > session)
{
    const auto request = session->get_request();
    std::string path = request->get_path();


    int content_length = request->get_header("Content-Length", 0);
    // C++ 11新标准：lambda函数
    session->fetch(content_length, [](const std::shared_ptr< restbed::Session > session, const restbed::Bytes& body)
        {

            fprintf(stdout, "%.*s\n", (int)body.size(), body.data());
        });
    session->close(restbed::OK, "Hello, World!", { { "Content-Length", "13" } });
}

void authorize_handler(const std::shared_ptr< restbed::Session> session)
{
    LOG_STACK_TRACE();

    boost::property_tree::ptree res_pt;
    try
    {
        const auto request = session->get_request();
        std::string path = request->get_path();
        int content_length = request->get_header("Content-Length", 0);
        // C++ 11新标准：lambda函数
        std::string delivery, user_name, pc_name;
        session->fetch(content_length, [&](const std::shared_ptr< restbed::Session > session, const restbed::Bytes& body)
            {
                std::string str_body(body.begin(), body.end());
                printf_s(str_body.c_str());
                std::stringstream str;
                str << str_body;
                boost::property_tree::ptree param;
                boost::property_tree::read_json(str, param);
                delivery = param.get<std::string>("delivery");
                user_name = param.get<std::string>("user_name");
                pc_name = param.get<std::string>("computer_name");
            });

        LOG_DEBUG(L"delivery = %S", delivery.c_str());
        std::wstring uuser_name, upc_name;
        jcvos::Utf8ToUnicode(uuser_name, user_name);
        jcvos::Utf8ToUnicode(upc_name, pc_name);
        LOG_DEBUG(L"user name = %s", uuser_name.c_str());

        CAuthorizeServiceApp* app = dynamic_cast<CAuthorizeServiceApp*>(jcvos::CJCAppBase::GetApp());

        std::string license;
        bool br = app->m_app_info.AuthorizeRegister(license, delivery, upc_name, uuser_name);
        if (!br)
        {
            res_pt.add("result", "ERROR");
            res_pt.add("message", "Unknown error");
        }
        else
        {
            res_pt.add<std::string>("result", "OK");
            res_pt.add<std::string>("license", license);
        }
    }
    catch (jcvos::CJCException& err)
    {
        res_pt.add<std::string>("result", "ERROR");
        res_pt.add("code", err.GetErrorID());
        res_pt.add("message", err.what());
    }
    catch (const std::exception& err)
    {
        res_pt.add<std::string>("result", "ERROR");
        res_pt.add("message", err.what());
    }
    catch (const std::runtime_error& re)
    {
        res_pt.add<std::string>("result", "ERROR");
        res_pt.add("message", re.what());
        //const auto error_handler = session->m_pimpl->get_error_handler();
        //error_handler(400, re, session);
    }

    std::stringstream out;
    boost::property_tree::write_json(out, res_pt);
    const std::string & str_body = out.str();

    const std::multimap< std::string, std::string > headers
    {
        { "Content-Type", "application/json"},
        { "Content-Length", std::to_string(str_body.length()) }
    };
    session->close(restbed::OK, str_body, headers);
}

void rebound_count_handler(const std::shared_ptr< restbed::Session> session)
{

}



int CAuthorizeServiceApp::Run(void)
{
    LoadConfig(m_config_fn);

    //auto resource = std::make_shared< restbed::Resource >();
    //resource->set_path("/resource");
    //resource->set_method_handler("GET", get_method_handler);

    auto authorize = std::make_shared< restbed::Resource >();
    authorize->set_path("/authorize");
    authorize->set_method_handler("POST", authorize_handler);

    auto rebound_count = std::make_shared<restbed::Resource>();
    rebound_count->set_path("/rebound_count");
    rebound_count->set_method_handler("GET", rebound_count_handler);

    auto settings = std::make_shared< restbed::Settings >();
    settings->set_port(1984);
    settings->set_default_header("Connection", "close");

    restbed::Service service;
//    service.publish(resource);
    service.publish(rebound_count);
    service.publish(authorize);
    service.start(settings);

    return EXIT_SUCCESS;
}