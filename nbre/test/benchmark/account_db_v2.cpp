// Copyright (C) 2018 go-nebulas authors
//
// This file is part of the go-nebulas library.
//
// the go-nebulas library is free software: you can redistribute it and/or
// modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// the go-nebulas library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the // GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the go-nebulas library.  If not, see
// <http://www.gnu.org/licenses/>.
//

#include "test/benchmark/account_db_v2.h"

namespace neb {
namespace fs {

account_db_v2::account_db_v2(neb::fs::account_db *adb_ptr)
    : m_adb_ptr(adb_ptr) {}

neb::wei_t account_db_v2::get_balance(const neb::address_t &addr,
                                      neb::block_height_t height) {
  return m_adb_ptr->get_balance(addr, height);
}

neb::address_t
account_db_v2::get_contract_deployer(const neb::address_t &addr,
                                     neb::block_height_t height) {
  return m_adb_ptr->get_contract_deployer(addr, height);
}

void account_db_v2::init_height_address_val_internal(
    neb::block_height_t start_block,
    const std::unordered_map<neb::address_t, neb::wei_t> &addr_balance) {

  for (auto &ele : addr_balance) {
    std::vector<neb::block_height_t> v{start_block};
    m_addr_height_list.insert(std::make_pair(ele.first, v));

    auto iter = m_height_addr_val.find(start_block);
    if (iter == m_height_addr_val.end()) {
      std::unordered_map<address_t, wei_t> addr_val = {{ele.first, ele.second}};
      m_height_addr_val.insert(std::make_pair(start_block, addr_val));
    } else {
      auto &addr_val = iter->second;
      addr_val.insert(std::make_pair(ele.first, ele.second));
    }
  }
}

void account_db_v2::update_height_address_val_internal(
    const std::vector<neb::fs::transaction_info_t> &txs,
    std::unordered_map<neb::address_t, neb::wei_t> &addr_balance) {

  if (txs.empty()) {
    return;
  }
  auto start_block = txs.front().m_height;
  init_height_address_val_internal(start_block, addr_balance);

  for (auto it = txs.begin(); it != txs.end(); it++) {
    address_t from = it->m_from;
    address_t to = it->m_to;

    block_height_t height = it->m_height;
    wei_t tx_value = it->m_tx_value;
    wei_t value = tx_value;

    if (addr_balance.find(from) == addr_balance.end()) {
      addr_balance.insert(std::make_pair(from, 0));
    }
    if (addr_balance.find(to) == addr_balance.end()) {
      addr_balance.insert(std::make_pair(to, 0));
    }

    int32_t status = it->m_status;
    if (status) {
      addr_balance[from] -= value;
      addr_balance[to] += value;
    }

    wei_t gas_used = it->m_gas_used;
    if (gas_used != 0) {
      wei_t gas_val = gas_used * it->m_gas_price;
      addr_balance[from] -= gas_val;
    }

    if (m_height_addr_val.find(height) == m_height_addr_val.end()) {
      std::unordered_map<address_t, wei_t> addr_val = {
          {from, addr_balance[from]}, {to, addr_balance[to]}};
      m_height_addr_val.insert(std::make_pair(height, addr_val));
    } else {
      std::unordered_map<address_t, wei_t> &addr_val =
          m_height_addr_val[height];
      if (addr_val.find(from) == addr_val.end()) {
        addr_val.insert(std::make_pair(from, addr_balance[from]));
      } else {
        addr_val[from] = addr_balance[from];
      }
      if (addr_val.find(to) == addr_val.end()) {
        addr_val.insert(std::make_pair(to, addr_balance[to]));
      } else {
        addr_val[to] = addr_balance[to];
      }
    }

    if (m_addr_height_list.find(from) == m_addr_height_list.end()) {
      std::vector<block_height_t> v{height};
      m_addr_height_list.insert(std::make_pair(from, v));
    } else {
      std::vector<block_height_t> &v = m_addr_height_list[from];
      // expect reading transactions order by height asc
      if (!v.empty() && v.back() < height) {
        v.push_back(height);
      }
    }

    if (m_addr_height_list.find(to) == m_addr_height_list.end()) {
      std::vector<block_height_t> v{height};
      m_addr_height_list.insert(std::make_pair(to, v));
    } else {
      std::vector<block_height_t> &v = m_addr_height_list[to];
      if (!v.empty() && v.back() < height) {
        v.push_back(height);
      }
    }
  }
}

neb::wei_t
account_db_v2::get_account_balance_internal(const neb::address_t &address,
                                            neb::block_height_t height) {

  auto addr_it = m_addr_height_list.find(address);
  if (addr_it == m_addr_height_list.end()) {
    return get_balance(address, height);
  }

  auto height_it =
      std::lower_bound(addr_it->second.begin(), addr_it->second.end(), height);

  if (height_it == addr_it->second.end()) {
    height_it--;
    return m_height_addr_val[*height_it][address];
  }

  if (height_it == addr_it->second.begin()) {
    if (*height_it == height) {
      return m_height_addr_val[*height_it][address];
    } else {
      return get_balance(address, height);
    }
  }

  if (*height_it != height) {
    height_it--;
  }

  return m_height_addr_val[*height_it][address];
}

neb::floatxx_t account_db_v2::get_normalized_value(neb::floatxx_t value) {
  uint64_t ratio = 1000000000000000000ULL;
  return value / neb::floatxx_t(ratio);
}

} // namespace fs
} // namespace neb
