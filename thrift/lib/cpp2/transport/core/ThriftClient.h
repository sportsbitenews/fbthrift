/*
 * Copyright 2017-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <stdint.h>
#include <thrift/lib/cpp2/transport/core/ClientConnectionIf.h>
#include <thrift/lib/cpp/protocol/TProtocolTypes.h>
#include <thrift/lib/cpp2/async/ClientChannel.h>
#include <map>
#include <memory>
#include <string>

namespace apache {
namespace thrift {

/**
 * This is the client side interface for Thrift RPCs.  You create an
 * object of this type and pass it as an argument to the constructor
 * of the client code generated by the Thrift compiler:
 *
 *   unique_ptr<ThriftClient> thriftClient = ...;
 *   unique_ptr<ChatRoomServiceAsyncClient> chatRoomClient =
 *       make_unique<ChatRoomServiceAsyncClient>(move(thriftClient));
 *   // now you can use chatRoomClient to perform RPCs.
 *
 * ThriftClient objects are lightweight and you can create a new one
 * for each RPC if you wish.  However RPCs that use the same event
 * base (thread) for callbacks can share the same ThriftClient object.
 *
 * TODO: We intend to support a special event base free ThriftClient
 * object in the future for synchronous-only RPCs.  Right now, support
 * for synchronous RPCs works exactly as with the existing Thrift
 * framework.
 *
 * ThriftClient objects are provided a ClientConnectionIf object as
 * parameter during construction.  This handles the lower level
 * connection aspects of the RPCs.  The ThriftClient and
 * ClientConnectionIf objects can either share the same event base, or
 * use different event bases.
 *
 * To create a ThriftClient object that shares the connection's event
 * base:
 *
 *   EventBase evb; // shared event base
 *   TAsyncTransport::UniquePtr transport(new TAsyncSocket(&evb, addr, port));
 *   auto connection = H2ClientConnection::newHTTP2Connection(move(transport));
 *   auto thriftClient = make_unique<ThriftClient>(move(connection));
 *
 * To manage the connections on their own threads and have RPC
 * callbacks on the application thread:
 *
 *   auto mgr = ConnectionManager.getInstance();
 *   auto connection = mgr.getConnection(addr, port);
 *   EventBase evb; // event base of application thread
 *   auto thriftClient = make_unique<ThriftClient>(connection, &evb);
 *
 * Note: The notion of "channel" in the base class "ClientChannel" below
 * is somewhat different from that of "ThriftChannelIf".
 */
class ThriftClient : public ClientChannel {
 public:
  // Use "Ptr" instead of "unique_ptr<ThriftClient>".
  using Ptr =
      std::unique_ptr<ThriftClient, folly::DelayedDestruction::Destructor>;

  // Creates a ThriftClient object that uses "connection".  Callbacks
  // for RPCs made using this object are run on "evb".
  ThriftClient(
      std::shared_ptr<ClientConnectionIf> connection,
      folly::EventBase* evb);

  // Creates a ThriftClient object that uses "connection".  Callbacks
  // for RPCs made using this object are run on the event base of the
  // connection.
  explicit ThriftClient(std::shared_ptr<ClientConnectionIf> connection);

  ThriftClient(const ThriftClient&) = delete;
  ThriftClient& operator=(const ThriftClient&) = delete;

  void setProtocolId(uint16_t protocolId);

  // begin RequestChannel methods

  uint32_t sendRequest(
      RpcOptions& rpcOptions,
      std::unique_ptr<RequestCallback> cb,
      std::unique_ptr<ContextStack> ctx,
      std::unique_ptr<folly::IOBuf> buf,
      std::shared_ptr<apache::thrift::transport::THeader> header) override;

  uint32_t sendOnewayRequest(
      RpcOptions& rpcOptions,
      std::unique_ptr<RequestCallback> cb,
      std::unique_ptr<ContextStack> ctx,
      std::unique_ptr<folly::IOBuf> buf,
      std::shared_ptr<apache::thrift::transport::THeader> header) override;

  // Returns the event base on which the callbacks must be scheduled.
  folly::EventBase* getEventBase() const override;

  uint16_t getProtocolId() override;

  void setCloseCallback(CloseCallback* cb) override;

  // end RequestChannel methods

  // Following methods are delegated to the connection object.  Given
  // that connection objects may be shared by multiple ThriftClient
  // objects, calls to these methods will affect all these objects.

  // begin ClientChannel methods
  apache::thrift::async::TAsyncTransport* getTransport() override;
  bool good() override;
  SaturationStatus getSaturationStatus() override;
  void attachEventBase(folly::EventBase* eventBase) override;
  void detachEventBase() override;
  bool isDetachable() override;
  bool isSecurityActive() override;
  uint32_t getTimeout() override;
  void setTimeout(uint32_t ms) override;
  void closeNow() override;
  CLIENT_TYPE getClientType() override;
  // end ClientChannel methods

 private:
  std::shared_ptr<ClientConnectionIf> connection_;
  folly::EventBase* evb_;
  uint16_t protocolId_{apache::thrift::protocol::T_COMPACT_PROTOCOL};

  // Destructor is private because this class inherits from
  // folly:DelayedDestruction.
  virtual ~ThriftClient() = default;

  uint32_t sendRequestHelper(
      RpcOptions& rpcOptions,
      bool oneway,
      std::unique_ptr<RequestCallback> cb,
      std::unique_ptr<ContextStack> ctx,
      std::unique_ptr<folly::IOBuf> buf,
      std::shared_ptr<apache::thrift::transport::THeader> header);

  void setRequestHeaderOptions(apache::thrift::transport::THeader* header);

  void setHeaders(
      std::map<std::string, std::string>& dstHeaders,
      const apache::thrift::transport::THeader::StringToStringMap& srcHeaders);

  std::unique_ptr<std::map<std::string, std::string>> buildHeaderMap(
      apache::thrift::transport::THeader* header);
};

} // namespace thrift
} // namespace apache
