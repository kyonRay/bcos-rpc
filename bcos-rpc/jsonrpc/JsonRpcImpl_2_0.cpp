/**
 *  Copyright (C) 2021 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @brief: implement for the RPC
 * @file: JsonRpcImpl_2_0.h
 * @author: octopus
 * @date: 2021-07-09
 */

#include "libutilities/DataConvertUtility.h"
#include <bcos-framework/interfaces/protocol/Transaction.h>
#include <bcos-framework/interfaces/protocol/TransactionReceipt.h>
#include <bcos-framework/libprotocol/LogEntry.h>
#include <bcos-framework/libutilities/Base64.h>
#include <bcos-framework/libutilities/Log.h>
#include <bcos-rpc/http/ws/WsVersion.h>
#include <bcos-rpc/jsonrpc/Common.h>
#include <bcos-rpc/jsonrpc/JsonRpcImpl_2_0.h>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>

using namespace std;
using namespace bcos;
using namespace bcos::rpc;
using namespace boost::iterators;
using namespace boost::archive::iterators;

void JsonRpcImpl_2_0::initMethod()
{
    m_methodToFunc["call"] =
        std::bind(&JsonRpcImpl_2_0::callI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["sendTransaction"] = std::bind(
        &JsonRpcImpl_2_0::sendTransactionI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getTransaction"] = std::bind(
        &JsonRpcImpl_2_0::getTransactionI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getTransactionReceipt"] = std::bind(&JsonRpcImpl_2_0::getTransactionReceiptI,
        this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getBlockByHash"] = std::bind(
        &JsonRpcImpl_2_0::getBlockByHashI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getBlockByNumber"] = std::bind(
        &JsonRpcImpl_2_0::getBlockByNumberI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getBlockHashByNumber"] = std::bind(&JsonRpcImpl_2_0::getBlockHashByNumberI,
        this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getBlockNumber"] = std::bind(
        &JsonRpcImpl_2_0::getBlockNumberI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getCode"] =
        std::bind(&JsonRpcImpl_2_0::getCodeI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getSealerList"] = std::bind(
        &JsonRpcImpl_2_0::getSealerListI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getObserverList"] = std::bind(
        &JsonRpcImpl_2_0::getObserverListI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getPbftView"] = std::bind(
        &JsonRpcImpl_2_0::getPbftViewI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getPendingTxSize"] = std::bind(
        &JsonRpcImpl_2_0::getPendingTxSizeI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getSyncStatus"] = std::bind(
        &JsonRpcImpl_2_0::getSyncStatusI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getSystemConfigByKey"] = std::bind(&JsonRpcImpl_2_0::getSystemConfigByKeyI,
        this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getTotalTransactionCount"] =
        std::bind(&JsonRpcImpl_2_0::getTotalTransactionCountI, this, std::placeholders::_1,
            std::placeholders::_2);
    m_methodToFunc["getPeers"] =
        std::bind(&JsonRpcImpl_2_0::getPeersI, this, std::placeholders::_1, std::placeholders::_2);
    m_methodToFunc["getNodeInfo"] = std::bind(
        &JsonRpcImpl_2_0::getNodeInfoI, this, std::placeholders::_1, std::placeholders::_2);

    for (const auto& method : m_methodToFunc)
    {
        RPC_IMPL_LOG(INFO) << LOG_BADGE("initMethod") << LOG_KV("method", method.first);
    }
    RPC_IMPL_LOG(INFO) << LOG_BADGE("initMethod") << LOG_KV("size", m_methodToFunc.size());
}

std::string JsonRpcImpl_2_0::encodeData(bcos::bytesConstRef _data)
{
    return base64Encode(_data);
}

std::shared_ptr<bcos::bytes> JsonRpcImpl_2_0::decodeData(const std::string& _data)
{
    return base64DecodeBytes(_data);
}

void JsonRpcImpl_2_0::parseRpcRequestJson(
    const std::string& _requestBody, JsonRequest& _jsonRequest)
{
    Json::Value root;
    Json::Reader jsonReader;
    std::string errorMessage;

    try
    {
        std::string jsonrpc = "";
        std::string method = "";
        int64_t id = 0;
        do
        {
            if (!jsonReader.parse(_requestBody, root))
            {
                errorMessage = "invalid request json object";
                break;
            }

            if (!root.isMember("jsonrpc"))
            {
                errorMessage = "request has no jsonrpc field";
                break;
            }
            jsonrpc = root["jsonrpc"].asString();

            if (!root.isMember("method"))
            {
                errorMessage = "request has no method field";
                break;
            }
            method = root["method"].asString();

            if (root.isMember("id"))
            {
                id = root["id"].asInt64();
            }

            if (!root.isMember("params"))
            {
                errorMessage = "request has no params field";
                break;
            }

            if (!root["params"].isArray())
            {
                errorMessage = "request params is not array object";
                break;
            }

            auto jParams = root["params"];

            _jsonRequest.jsonrpc = jsonrpc;
            _jsonRequest.method = method;
            _jsonRequest.id = id;
            _jsonRequest.params = jParams;

            // RPC_IMPL_LOG(DEBUG) << LOG_BADGE("parseRpcRequestJson") << LOG_KV("method", method)
            //                     << LOG_KV("requestMessage", _requestBody);

            // success return
            return;

        } while (0);
    }
    catch (const std::exception& e)
    {
        RPC_IMPL_LOG(ERROR) << LOG_BADGE("parseRpcRequestJson") << LOG_KV("request", _requestBody)
                            << LOG_KV("error", boost::diagnostic_information(e));
        BOOST_THROW_EXCEPTION(
            JsonRpcException(JsonRpcError::ParseError, "Invalid JSON was received by the server."));
    }

    RPC_IMPL_LOG(ERROR) << LOG_BADGE("parseRpcRequestJson") << LOG_KV("request", _requestBody)
                        << LOG_KV("errorMessage", errorMessage);

    BOOST_THROW_EXCEPTION(JsonRpcException(
        JsonRpcError::InvalidRequest, "The JSON sent is not a valid Request object."));
}

void JsonRpcImpl_2_0::parseRpcResponseJson(
    const std::string& _responseBody, JsonResponse& _jsonResponse)
{
    Json::Value root;
    Json::Reader jsonReader;
    std::string errorMessage;

    try
    {
        do
        {
            if (!jsonReader.parse(_responseBody, root))
            {
                errorMessage = "invalid response json object";
                break;
            }

            if (!root.isMember("jsonrpc"))
            {
                errorMessage = "response has no jsonrpc field";
                break;
            }
            _jsonResponse.jsonrpc = root["jsonrpc"].asString();

            if (!root.isMember("id"))
            {
                errorMessage = "response has no id field";
                break;
            }
            _jsonResponse.id = root["id"].asInt64();

            if (root.isMember("error"))
            {
                auto jError = root["error"];
                _jsonResponse.error.code = jError["code"].asInt64();
                _jsonResponse.error.message = jError["message"].asString();
            }

            if (root.isMember("result"))
            {
                _jsonResponse.result = root["result"];
            }

            RPC_IMPL_LOG(DEBUG) << LOG_BADGE("parseRpcResponseJson")
                                << LOG_KV("jsonrpc", _jsonResponse.jsonrpc)
                                << LOG_KV("id", _jsonResponse.id)
                                << LOG_KV("error", _jsonResponse.error.toString())
                                << LOG_KV("responseBody", _responseBody);

            return;
        } while (0);
    }
    catch (std::exception& e)
    {
        RPC_IMPL_LOG(ERROR) << LOG_BADGE("parseRpcResponseJson")
                            << LOG_KV("response", _responseBody)
                            << LOG_KV("error", boost::diagnostic_information(e));
        BOOST_THROW_EXCEPTION(
            JsonRpcException(JsonRpcError::ParseError, "Invalid JSON was received by the server."));
    }

    RPC_IMPL_LOG(ERROR) << LOG_BADGE("parseRpcResponseJson") << LOG_KV("response", _responseBody)
                        << LOG_KV("errorMessage", errorMessage);

    BOOST_THROW_EXCEPTION(JsonRpcException(
        JsonRpcError::InvalidRequest, "The JSON sent is not a valid Response object."));
}

std::string JsonRpcImpl_2_0::toStringResponse(const JsonResponse& _jsonResponse)
{
    auto jResp = toJsonResponse(_jsonResponse);
    Json::FastWriter writer;
    std::string resp = writer.write(jResp);
    return resp;
}

Json::Value JsonRpcImpl_2_0::toJsonResponse(const JsonResponse& _jsonResponse)
{
    Json::Value jResp;
    jResp["jsonrpc"] = _jsonResponse.jsonrpc;
    jResp["id"] = _jsonResponse.id;

    if (_jsonResponse.error.code == 0)
    {  // success
        jResp["result"] = _jsonResponse.result;
    }
    else
    {  // error
        Json::Value jError;
        jError["code"] = _jsonResponse.error.code;
        jError["message"] = _jsonResponse.error.message;
        jResp["error"] = jError;
    }

    return jResp;
}

void JsonRpcImpl_2_0::onRPCRequest(const std::string& _requestBody, Sender _sender)
{
    JsonRequest request;
    JsonResponse response;
    try
    {
        parseRpcRequestJson(_requestBody, request);

        response.jsonrpc = request.jsonrpc;
        response.id = request.id;

        const auto& method = request.method;
        auto it = m_methodToFunc.find(method);
        if (it == m_methodToFunc.end())
        {
            BOOST_THROW_EXCEPTION(JsonRpcException(
                JsonRpcError::MethodNotFound, "The method does not exist/is not available."));
        }

        it->second(request.params,
            [_requestBody, response, _sender](Error::Ptr _error, Json::Value& _result) mutable {
                if (_error && (_error->errorCode() != bcos::protocol::CommonError::SUCCESS))
                {
                    // error
                    response.error.code = _error->errorCode();
                    response.error.message = _error->errorMessage();
                }
                else
                {
                    response.result.swap(_result);
                }
                auto strResp = toStringResponse(response);
                _sender(strResp);
                RPC_IMPL_LOG(TRACE) << LOG_BADGE("onRPCRequest") << LOG_KV("request", _requestBody)
                                    << LOG_KV("response", strResp);
            });

        // success response
        return;
    }
    catch (const JsonRpcException& e)
    {
        response.error.code = e.code();
        response.error.message = std::string(e.what());
    }
    catch (const std::exception& e)
    {
        // server internal error or unexpected error
        response.error.code = JsonRpcError::InvalidRequest;
        response.error.message = std::string(e.what());
    }

    auto strResp = toStringResponse(response);
    // error response
    _sender(strResp);

    // RPC_IMPL_LOG(DEBUG) << LOG_BADGE("onRPCRequest") << LOG_KV("request", _requestBody)
    //                     << LOG_KV("response", strResp);
}

void JsonRpcImpl_2_0::toJsonResp(
    Json::Value& jResp, bcos::protocol::Transaction::ConstPtr _transactionPtr)
{
    // transaction version
    jResp["version"] = _transactionPtr->version();
    // transaction hash
    jResp["hash"] = toHexStringWithPrefix(_transactionPtr->hash());
    // transaction nonce
    jResp["nonce"] = _transactionPtr->nonce().str(16);
    // blockLimit
    jResp["blockLimit"] = _transactionPtr->blockLimit();
    // the receiver address
    jResp["to"] = string(_transactionPtr->to());
    // the sender address
    jResp["from"] = toHexStringWithPrefix(_transactionPtr->sender());
    // the input data
    jResp["input"] = toHexStringWithPrefix(_transactionPtr->input());
    // importTime
    jResp["importTime"] = _transactionPtr->importTime();
    // the chainID
    jResp["chainID"] = std::string(_transactionPtr->chainId());
    // the groupID
    jResp["groupID"] = std::string(_transactionPtr->groupId());
    // the signature
    jResp["signature"] = toHexStringWithPrefix(_transactionPtr->signatureData());
}

void JsonRpcImpl_2_0::toJsonResp(Json::Value& jResp, const std::string& _txHash,
    bcos::protocol::TransactionReceipt::ConstPtr _transactionReceiptPtr)
{
    jResp["version"] = _transactionReceiptPtr->version();
    jResp["contractAddress"] = string(_transactionReceiptPtr->contractAddress());
    jResp["gasUsed"] = _transactionReceiptPtr->gasUsed().str(16);
    jResp["status"] = _transactionReceiptPtr->status();
    jResp["blockNumber"] = _transactionReceiptPtr->blockNumber();
    jResp["output"] = toHexStringWithPrefix(_transactionReceiptPtr->output());
    jResp["transactionHash"] = _txHash;
    jResp["hash"] = _transactionReceiptPtr->hash().hexPrefixed();
    jResp["logEntries"] = Json::Value(Json::arrayValue);
    for (const auto& logEntry : _transactionReceiptPtr->logEntries())
    {
        Json::Value jLog;
        jLog["address"] = toHexStringWithPrefix(logEntry.address());
        jLog["topic"] = Json::Value(Json::arrayValue);
        for (const auto& topic : logEntry.topics())
        {
            jLog["topic"].append(topic.hexPrefixed());
        }
        jLog["data"] = toHexStringWithPrefix(logEntry.data());
        jResp["logEntries"].append(jLog);
    }
}


void JsonRpcImpl_2_0::toJsonResp(
    Json::Value& jResp, bcos::protocol::BlockHeader::Ptr _blockHeaderPtr)
{
    if (!_blockHeaderPtr)
    {
        return;
    }

    jResp["hash"] = toHexStringWithPrefix(_blockHeaderPtr->hash());
    jResp["version"] = _blockHeaderPtr->version();
    jResp["txsRoot"] = toHexStringWithPrefix(_blockHeaderPtr->txsRoot());
    jResp["receiptsRoot"] = toHexStringWithPrefix(_blockHeaderPtr->receiptsRoot());
    jResp["stateRoot"] = toHexStringWithPrefix(_blockHeaderPtr->stateRoot());
    jResp["number"] = _blockHeaderPtr->number();
    jResp["gasUsed"] = _blockHeaderPtr->gasUsed().str(16);
    jResp["timestamp"] = _blockHeaderPtr->timestamp();
    jResp["sealer"] = _blockHeaderPtr->sealer();
    jResp["extraData"] = toHexStringWithPrefix(_blockHeaderPtr->extraData());

    jResp["consensusWeights"] = Json::Value(Json::arrayValue);
    for (const auto& wei : _blockHeaderPtr->consensusWeights())
    {
        jResp["consensusWeights"].append(wei);
    }

    jResp["sealerList"] = Json::Value(Json::arrayValue);
    for (const auto& sealer : _blockHeaderPtr->sealerList())
    {
        jResp["sealerList"].append(toHexStringWithPrefix(sealer));
    }

    Json::Value jParentInfo(Json::arrayValue);
    for (const auto& p : _blockHeaderPtr->parentInfo())
    {
        Json::Value jp;
        jp["blockNumber"] = p.blockNumber;
        jp["blockHash"] = toHexStringWithPrefix(p.blockHash);
        jParentInfo.append(jp);
    }
    jResp["parentInfo"] = jParentInfo;

    Json::Value jSignList(Json::arrayValue);
    for (const auto& sign : _blockHeaderPtr->signatureList())
    {
        Json::Value jSign;
        jSign["sealerIndex"] = sign.index;
        jSign["signature"] = toHexStringWithPrefix(sign.signature);
        jSignList.append(jSign);
    }
    jResp["signatureList"] = jSignList;
}

void JsonRpcImpl_2_0::toJsonResp(
    Json::Value& jResp, bcos::protocol::Block::Ptr _blockPtr, bool _onlyTxHash)
{
    if (!_blockPtr)
    {
        return;
    }

    // header
    toJsonResp(jResp, _blockPtr->blockHeader());
    auto txSize = _blockPtr->transactionsSize();

    Json::Value jTxs(Json::arrayValue);
    for (std::size_t index = 0; index < txSize; ++index)
    {
        Json::Value jTx;
        if (_onlyTxHash)
        {
            jTx = toHexStringWithPrefix(_blockPtr->transactionHash(index));
        }
        else
        {
            toJsonResp(jTx, _blockPtr->transaction(index));
        }
        jTxs.append(jTx);
    }

    jResp["transactions"] = jTxs;
}

void JsonRpcImpl_2_0::call(const std::string& _to, const std::string& _data, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("call") << LOG_KV("to", _to);

    auto transaction =
        m_transactionFactory->createTransaction(0, _to, *decodeData(_data), u256(0), 0, "", "", 0);

    m_executorInterface->asyncExecuteTransaction(
        transaction, [_to, _respFunc](const Error::Ptr& _error,
                         const protocol::TransactionReceipt::ConstPtr& _transactionReceiptPtr) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp["blockNumber"] = _transactionReceiptPtr->blockNumber();
                jResp["status"] = _transactionReceiptPtr->status();
                jResp["output"] = toHexStringWithPrefix(_transactionReceiptPtr->output());
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("call") << LOG_KV("to", _to)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::sendTransaction(
    const std::string& _data, bool _requireProof, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("sendTransaction");

    auto self = std::weak_ptr<JsonRpcImpl_2_0>(shared_from_this());
    auto transactionDataPtr = decodeData(_data);
    m_txPoolInterface->asyncSubmit(transactionDataPtr,
        [_requireProof, _data, _respFunc, self](Error::Ptr _error,
            bcos::protocol::TransactionSubmitResult::Ptr _transactionSubmitResult) {
            if (_error && _error->errorCode() != bcos::protocol::CommonError::SUCCESS)
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("sendTransaction") << LOG_KV("data", _data)
                    << LOG_KV("requireProof", _requireProof)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
                Json::Value jResp;
                _respFunc(_error, jResp);
                return;
            }

            // get transaction receipt
            auto txHash = _transactionSubmitResult->txHash();
            auto hexPreTxHash = txHash.hexPrefixed();

            RPC_IMPL_LOG(TRACE) << LOG_BADGE("sendTransaction") << LOG_DESC("getTransactionReceipt")
                                << LOG_KV("hexPreTxHash", hexPreTxHash)
                                << LOG_KV("requireProof", _requireProof);

            auto rpc = self.lock();
            if (!rpc)
            {
                return;
            }

            // fetch transaction receipt
            rpc->getTransactionReceipt(hexPreTxHash, _requireProof,
                [rpc, _requireProof, hexPreTxHash, _respFunc](
                    bcos::Error::Ptr _error, Json::Value& jReceipt) {
                    if (_error && _error->errorCode() != bcos::protocol::CommonError::SUCCESS)
                    {
                        RPC_IMPL_LOG(WARNING)
                            << LOG_BADGE("sendTransaction") << LOG_DESC("getTransactionReceipt")
                            << LOG_KV("hexPreTxHash", hexPreTxHash)
                            << LOG_KV("requireProof", _requireProof)
                            << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                            << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
                        _respFunc(_error, jReceipt);
                        return;
                    }

                    // fetch transaction proof
                    rpc->getTransaction(hexPreTxHash, _requireProof,
                        [jReceipt, hexPreTxHash, _respFunc](
                            bcos::Error::Ptr _error, Json::Value& _jTx) {
                            auto jReceiptCopy = jReceipt;
                            if (_error &&
                                _error->errorCode() != bcos::protocol::CommonError::SUCCESS)
                            {
                                RPC_IMPL_LOG(WARNING)
                                    << LOG_BADGE("sendTransaction") << LOG_DESC("getTransaction")
                                    << LOG_KV("hexPreTxHash", hexPreTxHash)
                                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                                    << LOG_KV("errorMessage",
                                           _error ? _error->errorMessage() : "success");
                            }

                            if (_jTx.isMember("transactionProof"))
                            {
                                jReceiptCopy["transactionProof"] = _jTx["transactionProof"];
                            }

                            if (_jTx.isMember("input"))
                            {
                                jReceiptCopy["input"] = _jTx["input"];
                            }

                            _respFunc(nullptr, jReceiptCopy);
                        });
                });
        });
}


void JsonRpcImpl_2_0::addProofToResponse(
    Json::Value& jResp, std::string const& _key, ledger::MerkleProofPtr _merkleProofPtr)
{
    if (!_merkleProofPtr)
    {
        return;
    }

    RPC_IMPL_LOG(TRACE) << LOG_DESC("addProofToResponse") << LOG_KV("key", _key)
                        << LOG_KV("key", _key) << LOG_KV("merkleProofPtr", _merkleProofPtr->size());

    uint32_t index = 0;
    for (const auto& merkleItem : *_merkleProofPtr)
    {
        jResp[_key][index]["left"] = Json::arrayValue;
        jResp[_key][index]["right"] = Json::arrayValue;
        const auto& left = merkleItem.first;
        for (const auto& item : left)
        {
            jResp[_key][index]["left"].append(item);
        }

        const auto& right = merkleItem.second;
        for (const auto& item : right)
        {
            jResp[_key][index]["right"].append(item);
        }
        ++index;
    }
}

void JsonRpcImpl_2_0::getTransaction(
    const std::string& _txHash, bool _requireProof, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getTransaction") << LOG_KV("txHash", _txHash)
                        << LOG_KV("requireProof", _requireProof);

    bcos::crypto::HashListPtr hashListPtr = std::make_shared<bcos::crypto::HashList>();
    hashListPtr->push_back(bcos::crypto::HashType(_txHash));
    m_ledgerInterface->asyncGetBatchTxsByHashList(hashListPtr, _requireProof,
        [_txHash, _requireProof, _respFunc](Error::Ptr _error,
            bcos::protocol::TransactionsPtr _transactionsPtr,
            std::shared_ptr<std::map<std::string, ledger::MerkleProofPtr>> _transactionProofsPtr) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                if (!_transactionsPtr->empty())
                {
                    auto transactionPtr = (*_transactionsPtr)[0];
                    toJsonResp(jResp, transactionPtr);
                }

                RPC_IMPL_LOG(TRACE)
                    << LOG_DESC("getTransaction") << LOG_KV("txHash", _txHash)
                    << LOG_KV("requireProof", _requireProof)
                    << LOG_KV("transactionProofsPtr size",
                           (_transactionProofsPtr ? (int64_t)_transactionProofsPtr->size() : -1));


                if (_requireProof && _transactionProofsPtr && !_transactionProofsPtr->empty())
                {
                    auto transactionProofPtr = _transactionProofsPtr->begin()->second;
                    addProofToResponse(jResp, "transactionProof", transactionProofPtr);
                }
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getTransaction") << LOG_KV("txHash", _txHash)
                    << LOG_KV("requireProof", _requireProof)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getTransactionReceipt(
    const std::string& _txHash, bool _requireProof, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getTransactionReceipt") << LOG_KV("txHash", _txHash)
                        << LOG_KV("requireProof", _requireProof);

    auto hash = bcos::crypto::HashType(_txHash);

    auto self = std::weak_ptr<JsonRpcImpl_2_0>(shared_from_this());
    m_ledgerInterface->asyncGetTransactionReceiptByHash(hash, _requireProof,
        [_txHash, hash, _requireProof, _respFunc, self](Error::Ptr _error,
            protocol::TransactionReceipt::ConstPtr _transactionReceiptPtr,
            ledger::MerkleProofPtr _merkleProofPtr) {
            auto rpc = self.lock();
            if (!rpc)
            {
                return;
            }
            Json::Value jResp;
            if (_error && (_error->errorCode() != bcos::protocol::CommonError::SUCCESS))
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getTransactionReceipt") << LOG_KV("txHash", _txHash)
                    << LOG_KV("requireProof", _requireProof)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");

                _respFunc(_error, jResp);
                return;
            }

            toJsonResp(jResp, hash.hexPrefixed(), _transactionReceiptPtr);

            RPC_IMPL_LOG(TRACE) << LOG_DESC("getTransactionReceipt") << LOG_KV("txHash", _txHash)
                                << LOG_KV("requireProof", _requireProof)
                                << LOG_KV("merkleProofPtr", _merkleProofPtr);

            if (_requireProof && _merkleProofPtr)
            {
                addProofToResponse(jResp, "receiptProof", _merkleProofPtr);
            }

            // fetch transaction proof
            rpc->getTransaction(_txHash, _requireProof,
                [jResp, _txHash, _respFunc](bcos::Error::Ptr _error, Json::Value& _jTx) {
                    auto jRespCopy = jResp;
                    if (_error && _error->errorCode() != bcos::protocol::CommonError::SUCCESS)
                    {
                        RPC_IMPL_LOG(WARNING)
                            << LOG_BADGE("getTransactionReceipt") << LOG_DESC("getTransaction")
                            << LOG_KV("hexPreTxHash", _txHash)
                            << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                            << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
                    }

                    if (_jTx.isMember("input"))
                    {
                        jRespCopy["input"] = _jTx["input"];
                    }

                    _respFunc(nullptr, jRespCopy);
                });
        });
}

void JsonRpcImpl_2_0::getBlockByHash(
    const std::string& _blockHash, bool _onlyHeader, bool _onlyTxHash, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getBlockByHash") << LOG_KV("blockHash", _blockHash)
                        << LOG_KV("onlyHeader", _onlyHeader) << LOG_KV("onlyTxHash", _onlyTxHash);

    auto self = std::weak_ptr<JsonRpcImpl_2_0>(shared_from_this());
    m_ledgerInterface->asyncGetBlockNumberByHash(bcos::crypto::HashType(_blockHash),
        [_blockHash, _onlyHeader, _onlyTxHash, _respFunc, self](
            Error::Ptr _error, protocol::BlockNumber blockNumber) {
            if (!_error || _error->errorCode() == bcos::protocol::CommonError::SUCCESS)
            {
                auto rpc = self.lock();
                if (rpc)
                {
                    // call getBlockByNumber
                    return rpc->getBlockByNumber(blockNumber, _onlyHeader, _onlyTxHash, _respFunc);
                }
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getBlockByHash") << LOG_KV("blockHash", _blockHash)
                    << LOG_KV("onlyHeader", _onlyHeader) << LOG_KV("onlyTxHash", _onlyTxHash)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
                Json::Value jResp;
                _respFunc(_error, jResp);
            }
        });
}

void JsonRpcImpl_2_0::getBlockByNumber(
    int64_t _blockNumber, bool _onlyHeader, bool _onlyTxHash, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getBlockByNumber") << LOG_KV("_blockNumber", _blockNumber)
                        << LOG_KV("_onlyHeader", _onlyHeader) << LOG_KV("onlyTxHash", _onlyTxHash);

    m_ledgerInterface->asyncGetBlockDataByNumber(_blockNumber,
        _onlyHeader ? bcos::ledger::HEADER : bcos::ledger::HEADER | bcos::ledger::TRANSACTIONS,
        [_blockNumber, _onlyHeader, _onlyTxHash, _respFunc](
            Error::Ptr _error, protocol::Block::Ptr _block) {
            Json::Value jResp;
            if (_error && _error->errorCode() != bcos::protocol::CommonError::SUCCESS)
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getBlockByNumber") << LOG_KV("blockNumber", _blockNumber)
                    << LOG_KV("onlyHeader", _onlyHeader) << LOG_KV("onlyTxHash", _onlyTxHash)
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }
            else
            {
                if (_onlyHeader)
                {
                    toJsonResp(jResp, _block ? _block->blockHeader() : nullptr);
                }
                else
                {
                    toJsonResp(jResp, _block, _onlyTxHash);
                }
            }
            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getBlockHashByNumber(int64_t _blockNumber, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getBlockHashByNumber") << LOG_KV("blockNumber", _blockNumber);

    m_ledgerInterface->asyncGetBlockHashByNumber(
        _blockNumber, [_respFunc](Error::Ptr _error, crypto::HashType const& _hashValue) {
            if (_error && (_error->errorCode() != bcos::protocol::CommonError::SUCCESS))
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getBlockHashByNumber")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            Json::Value jResp = _hashValue.hexPrefixed();
            _respFunc(nullptr, jResp);
        });
}

void JsonRpcImpl_2_0::getBlockNumber(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getBlockNumber");

    m_ledgerInterface->asyncGetBlockNumber(
        [_respFunc](Error::Ptr _error, protocol::BlockNumber _blockNumber) {
            if (_error && (_error->errorCode() != bcos::protocol::CommonError::SUCCESS))
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getBlockNumber")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success")
                    << LOG_KV("blockNumber", _blockNumber);
            }

            Json::Value jResp = _blockNumber;
            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getCode(const std::string _contractAddress, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getCode") << LOG_KV("contractAddress", _contractAddress);

    m_executorInterface->asyncGetCode(
        std::string_view(_contractAddress), [_contractAddress, _respFunc](const Error::Ptr& _error,
                                                const std::shared_ptr<bytes>& _codeData) {
            std::string code;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                if (_codeData)
                {
                    code = toHexStringWithPrefix(
                        bcos::bytesConstRef(_codeData->data(), _codeData->size()));
                }
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getCode") << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success")
                    << LOG_KV("contractAddress", _contractAddress);
            }

            Json::Value jResp = code;
            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getSealerList(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getSealerList");

    m_ledgerInterface->asyncGetNodeListByType(bcos::ledger::CONSENSUS_SEALER,
        [_respFunc](Error::Ptr _error, consensus::ConsensusNodeListPtr _consensusNodeListPtr) {
            Json::Value jResp = Json::Value(Json::arrayValue);
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                if (_consensusNodeListPtr)
                {
                    for (const auto& consensusNodePtr : *_consensusNodeListPtr)
                    {
                        Json::Value node;
                        node["nodeID"] = consensusNodePtr->nodeID()->hex();
                        node["weight"] = consensusNodePtr->weight();
                        jResp.append(node);
                    }
                }
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getSealerList")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getObserverList(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getObserverList");

    m_ledgerInterface->asyncGetNodeListByType(bcos::ledger::CONSENSUS_OBSERVER,
        [_respFunc](Error::Ptr _error, consensus::ConsensusNodeListPtr _consensusNodeListPtr) {
            Json::Value jResp = Json::Value(Json::arrayValue);
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                if (_consensusNodeListPtr)
                {
                    for (const auto& consensusNodePtr : *_consensusNodeListPtr)
                    {
                        jResp.append(consensusNodePtr->nodeID()->hex());
                    }
                }
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getObserverList")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getPbftView(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getPbftView");

    m_consensusInterface->asyncGetPBFTView(
        [_respFunc](Error::Ptr _error, bcos::consensus::ViewType _viewValue) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp = _viewValue;
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getPbftView")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getPendingTxSize(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getPendingTxSize");

    m_txPoolInterface->asyncGetPendingTransactionSize(
        [_respFunc](Error::Ptr _error, size_t _pendingTxSize) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp = (int64_t)_pendingTxSize;
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getPendingTxSize")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getSyncStatus(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_BADGE("getSyncStatus");

    m_blockSyncInterface->asyncGetSyncInfo([_respFunc](Error::Ptr _error, std::string _syncStatus) {
        Json::Value jResp;
        if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
        {
            jResp = _syncStatus;
        }
        else
        {
            RPC_IMPL_LOG(ERROR) << LOG_BADGE("getSyncStatus")
                                << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                                << LOG_KV(
                                       "errorMessage", _error ? _error->errorMessage() : "success");
        }
        _respFunc(_error, jResp);
    });
}

void JsonRpcImpl_2_0::getSystemConfigByKey(const std::string& _keyValue, RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getSystemConfigByKey") << LOG_KV("keyValue", _keyValue);

    m_ledgerInterface->asyncGetSystemConfigByKey(_keyValue,
        [_respFunc](Error::Ptr _error, std::string _value, protocol::BlockNumber _blockNumber) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp["blockNumber"] = _blockNumber;
                jResp["value"] = _value;
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("asyncGetSystemConfigByKey")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getTotalTransactionCount(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getTotalTransactionCount");

    m_ledgerInterface->asyncGetTotalTransactionCount(
        [_respFunc](Error::Ptr _error, int64_t _totalTxCount, int64_t _failedTxCount,
            protocol::BlockNumber _latestBlockNumber) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp["blockNumber"] = _latestBlockNumber;
                jResp["transactionCount"] = _totalTxCount;
                jResp["failedTransactionCount"] = _failedTxCount;
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getTotalTransactionCount")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getPeers(RespFunc _respFunc)
{
    RPC_IMPL_LOG(TRACE) << LOG_DESC("getPeers");
    m_gatewayInterface->asyncGetPeers(
        [_respFunc](Error::Ptr _error, const std::string& _peersInfo) {
            Json::Value jResp;
            if (!_error || (_error->errorCode() == bcos::protocol::CommonError::SUCCESS))
            {
                jResp = _peersInfo;
            }
            else
            {
                RPC_IMPL_LOG(ERROR)
                    << LOG_BADGE("getPeers")
                    << LOG_KV("errorCode", _error ? _error->errorCode() : 0)
                    << LOG_KV("errorMessage", _error ? _error->errorMessage() : "success");
            }

            _respFunc(_error, jResp);
        });
}

void JsonRpcImpl_2_0::getNodeInfo(RespFunc _respFunc)
{
    Json::Value jResp;
    jResp["version"] = m_nodeInfo.version;
    jResp["wasm"] = m_nodeInfo.isWasm;
    jResp["smCrypto"] = m_nodeInfo.isSM;
    jResp["nodeID"] = m_nodeInfo.nodeID;
    jResp["chainID"] = m_nodeInfo.chainID;
    jResp["groupID"] = m_nodeInfo.groupID;
    jResp["agency"] = m_nodeInfo.agency;
    jResp["buildTime"] = m_nodeInfo.buildTime;
    jResp["gitCommit"] = m_nodeInfo.gitCommitHash;
    jResp["supportedVersion"] = m_nodeInfo.supportedVersion;
    jResp["wsProtocolVersion"] = bcos::ws::WsProtocolVersion::current;

    _respFunc(nullptr, jResp);
}