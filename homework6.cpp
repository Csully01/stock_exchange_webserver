/* Copyright Colton Sullivan 2023
 * A simple online stock exchange web-server.  
 * 
 * This multithreaded web-server performs simple stock trading
 * transactions on stocks.  Stocks must be maintained in an
 * unordered_map.
 *
 */

// The commonly used headers are included.  Of course, you may add any
// additional headers as needed.
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iomanip>
#include <vector>
#include <boost/format.hpp>
#include "Stock.h"

// Setup a server socket to accept connections on the socket
using namespace boost::asio;
using namespace boost::asio::ip;

// Shortcut to smart pointer with TcpStream
using TcpStreamPtr = std::shared_ptr<tcp::iostream>;

// Prototype for helper method defined in main.cpp
std::string url_decode(std::string);

using Guard = std::scoped_lock<std::mutex>;

const std::string HTTPRespHeader = "HTTP/1.1 200 OK\r\n"
    "Server: SimpleServer\r\n"
    "Content-Length: %1%\r\n"
    "Connection: Close\r\n"
    "Content-Type: text/html\r\n\r\n";

// The name space to hold all of the information that is shared
// between multiple threads.
namespace sm {
    // Unordered map including stock's name as the key (std::string)
    // and the actual Stock entry as the value.
    std::unordered_map<std::string, Stock> stockMap;

}  // namespace sm

/**
 * createS creates a new stock and will show that the stock exists if it 
 * already exists
 * 
 * @param stock is a string that holds the current stock
 * @param amount is an int that holds the amount being put into the stock
 * @param ret is a string that is being returned
 * 
 */
void createS(std::string stock, int amount, std::string& ret) {
        if (sm::stockMap.find(stock) != sm::stockMap.end()) {
            ret = "Stock " + stock + " already exists";
        } else {
            Guard lock(sm::stockMap[stock].mutex);
            sm::stockMap[stock].name = stock;
            sm::stockMap[stock].balance = amount;
            ret = "Stock " + stock + " created with balance = " + 
            std::to_string(amount);
        }
    }
    /**
     * buyS checks the stock being bought and shows that stock being updated
     * 
     * @param stock is a string that holds the current stock
     * @param amount is an int that holds the amount being sold
     * @param ret is a string that is being returned
     */
    void buyS(std::string stock, int amount, std::string& ret) {
        if (sm::stockMap.find(stock) == sm::stockMap.end()) {
        ret = "Stock not found";
        } else {
            Guard lock(sm::stockMap[stock].mutex);
            sm::stockMap[stock].balance -= amount;
            ret = "Stock " + stock + "'s balance updated";
        }
    }
    /**
     * sellS checks the stock begin sold and and shows that stock being 
     * updated
     * 
     * @param stock is a string that holds the current stock
     * @param amount is an int that holds the ammount being sold
     * @param ret is a string that is being returned
     */
    void sellS(std::string stock, int amount, std::string& ret) {
       if (sm::stockMap.find(stock) == sm::stockMap.end()) {
        ret = "Stock not found";
        } else {
            Guard lock(sm::stockMap[stock].mutex);
            sm::stockMap[stock].balance += amount;
            ret = "Stock " + stock + "'s balance updated";
        }
    }
    /**
     * statusS checks the status of the stock and the shows the 
     * balance for that stock
     * 
     * @param stock is a string that holds the current stock
     * @param ret is a string that is being returned
     */
    void statusS(std::string stock, std::string& ret) {
        if (sm::stockMap.find(stock) == sm::stockMap.end()) {
        ret = "Stock not found";
        } else {
             ret = "Balance for stock " + stock + " = " + 
            std::to_string(sm::stockMap[stock].balance);
        }
    }

    void resetS(std::string& ret) {
        sm::stockMap.clear();
        ret = "Stocks reset";
    }

/**
 * process runs the checking of what option it is for the stock
 * 
 * @param trans is a string of the transaction
 * @param stock is a string that stores the current stock
 * @param a is the ammount for each of the buy or sell
 */
std::string process(std::string trans, std::string stock, int a) {
    std::string ret = "";
    if (trans == "create") {
        createS(stock, a, ret);
    } else if (trans == "buy") {
        buyS(stock, a, ret);
    } else if (trans == "sell") {
        sellS(stock, a, ret);
    } else if (trans == "status") {
        statusS(stock, ret);
    } else if (trans == "reset") {
        resetS(ret);
    } else {
        ret = "Invalid request";
    }
    return ret;
}

/**
 * This method is called from a separate detached/background thread
 * from the runServer() method.  This method processes 1 transaction
 * from a client.  This method extracts the transaction information
 * and processes the transaction by calling the processTrans helper
 * method.
 * 
 * \param[in] is The input stream from where the HTTP request is to be
 * read and processed.
 *
 * \param[out] os The output stream to where the HTTP response is to
 * be written.
 */
void clientThread(std::istream& is, std::ostream& os) {
    // Read the HTTP request from the client, process it, and send an
    // HTTP response back to the client.
    unsigned int a;
    std::string line, url, x, stock, trans;
    std::getline(is, line);
    std::istringstream(line) >> url >> url;
    url = url_decode(url);
    std::replace(url.begin(), url.end(), '&', ' ');
    std::replace(url.begin(), url.end(), '=', ' ');
    std::istringstream(url) >> x >> trans >> x >> stock >> x >> a;
    for (std::string hdr; std::getline(is, hdr) && (hdr != "") && (hdr != 
    "\r");) {}
    const std::string data = process(trans, stock, a);

    // Formatting
    auto httpForm = boost::str(boost::format(HTTPRespHeader) % data.size());

    // result
    os << httpForm << data;
}

/**
 * Top-level method to run a custom HTTP server to process stock trade
 * requests.
 *
 * Phase 1: Multithreading is not needed.
 *
 * Phase 2: This method must use multiple threads -- each request
 * should be processed using a separate detached thread. Optional
 * feature: Limit number of detached-threads to be <= maxThreads.
 *
 * \param[in] server The boost::tcp::acceptor object to be used to accept
 * connections from various clients.
 *
 * \param[in] maxThreads The maximum number of threads that the server
 * should use at any given time.
 */
void runServer(tcp::acceptor& server, const int maxThreads) {
    // Process client connections one-by-one...forever
    while (true) {
        // Creates garbage-collected connection on heap 
        TcpStreamPtr client = std::make_shared<tcp::iostream>();
        server.accept(*client->rdbuf());  // wait for client to connect
        // Now we have a I/O stream to talk to the client. Have a
        // conversation using the protocol.
        std::thread thr([client]{ clientThread(*client, *client); });
        thr.detach();  // Process transaction independently
    }
}

// End of source code
