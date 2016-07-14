/* 
 * File:   main.cpp
 * Author: mayur
 *
 * Created on 13 July, 2016, 11:58 PM
 */

#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/ThreadPool.h"
#include "Poco/Thread.h"
#include "Poco/Runnable.h"
#include "Poco/Mutex.h"
#include "Poco/Random.h"
#include "Poco/AutoPtr.h"
#include "Poco/URI.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/StreamCopier.h"
#include <iostream>


using Poco::Notification;
using Poco::NotificationQueue;
using Poco::ThreadPool;
using Poco::Thread;
using Poco::Runnable;
using Poco::FastMutex;
using Poco::Mutex;
using Poco::AutoPtr;
using Poco::Net::HTTPClientSession;
using Poco::URI;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPMessage;
using Poco::Exception;
using Poco::Net::HTTPResponse;
using Poco::StreamCopier;
/*
 * 
 */

class Response {
public:
    Response(std::string s){
        resStr = s;
    }
    void setResponseString(std::string s){
        resStr = s;
    }
    std::string getResponseString(){
        return resStr;
    }
private:
    std::string resStr;
};


class ResponseNotification: public Notification
        // The notification sent to worker threads.
{
public:
        typedef AutoPtr<ResponseNotification> Ptr;

        ResponseNotification(Response *res):
                response(res)
        {
        }
        std::string getResponse() const{
            return response->getResponseString();
        }
private:
        Response* response;
};


class CommonMutex{
public:
    static FastMutex commonMutex;
};

class Worker: public Runnable
        // A worker thread that gets work items
        // from a NotificationQueue.
{
public:
        Worker(const std::string& name, NotificationQueue& queue):
                _name(name),
                _queue(queue)
        {
        }

        void run()
        {
                Poco::Random rnd;
                for (;;)
                {
                        Notification::Ptr pNf(_queue.waitDequeueNotification());
                        if (pNf)
                        {
                                ResponseNotification::Ptr pResponseNf = pNf.cast<ResponseNotification>();
                                if (pResponseNf)
                                {
                                        {
                                                FastMutex::ScopedLock lock(CommonMutex::commonMutex);
                                                std::cout << _name << " got response " << pResponseNf->getResponse() << std::endl;
                                        }
                                }
                        }
                        else break;
                }
        }

private:
        std::string        _name;
        NotificationQueue& _queue;
};

class Post{
public:
    std::string doGet(int id){
        std::string result;
        URI uri("http://jsonplaceholder.typicode.com/posts/" + std::to_string(id));
        std::string path(uri.getPathAndQuery());
        HTTPClientSession session(uri.getHost(), uri.getPort());
        HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
        HTTPResponse response;
        session.sendRequest(request);
        std::istream& rs = session.receiveResponse(response);
        if(response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK){
            StreamCopier::copyToString(rs, result);
            return result;
        }
        else
            return "";
    }
};


class MyProducer: public Runnable
        // A worker thread that gets work items
        // from a NotificationQueue.
{
private:
        std::string        _name;
        NotificationQueue& _queue;
public:
        MyProducer(const std::string& name, NotificationQueue& queue):
                _name(name),
                _queue(queue)
        {
        }

        void run()
        {
            Post p;
            Poco::Random rnd;
           
            for(int i = 1; i < 50; i++){
                {
                    
                    int id = rnd.next(100);
                    Response *res = new Response(p.doGet(id));
                    _queue.enqueueNotification(new ResponseNotification(res));
                    {
                        FastMutex::ScopedLock lock(CommonMutex::commonMutex);
                        std::cout<<_name<<" created request for id "<<id<<std::endl;
                    }
                }
                
            }
            std::cout<<"Producer completed it's job"<<std::endl;
        }


};

FastMutex CommonMutex::commonMutex;


int main(int argc, char** argv)
{
        NotificationQueue queue;

        // create some worker threads
        Worker worker1("Worker 1", queue);
        Worker worker2("Worker 2", queue);
        Worker worker3("Worker 3", queue);
        MyProducer producer1("Producer1", queue);
        Thread tWorker1, tWorker2, tWorker3, tProducer1;
        tWorker1.start(worker1);
        tWorker2.start(worker2);
        tWorker3.start(worker3);
        
        //Thread::sleep(3000);
        tProducer1.start(producer1);
        tProducer1.join();
        Thread::sleep(20000);

        // stop all worker threads
        queue.wakeUpAll();
        

        return 0;
}


