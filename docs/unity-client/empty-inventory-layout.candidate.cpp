#include <cstddef>
#include <CacheLib/CacheStorageInventory.h>
static_assert(sizeof(Core::MsgInventoryItem)==8);
static_assert(sizeof(Cache::InventoryData)==360);
static_assert(offsetof(Cache::InventoryData,count)==0);
static_assert(offsetof(Cache::InventoryData,items)==4);
static_assert(offsetof(Cache::InventoryData,recentEventIds)==88);
static_assert(offsetof(Cache::InventoryData,head)==344);
static_assert(offsetof(Cache::InventoryData,tail)==345);
static_assert(offsetof(Cache::InventoryData,lastOp)==346);
static_assert(offsetof(Cache::InventoryData,lastLsn)==352);
static_assert(Cache::EMPTY_INVENTORY.count==0);
static_assert(Cache::EMPTY_INVENTORY.head==0 && Cache::EMPTY_INVENTORY.tail==0);
static_assert(!Cache::EMPTY_INVENTORY.lastOp && Cache::EMPTY_INVENTORY.lastLsn==0);

#include <cstring>
int main(){
 unsigned char blob[360]{}; Cache::InventoryData data;
 std::memcpy(&data,blob,sizeof(data));
 if(data.count||data.head||data.tail||data.lastOp||data.lastLsn)return 1;
 for(const auto& item:data.items)if(item.itemID||item.quantity||item.slot)return 2;
 for(auto id:data.recentEventIds)if(id)return 3;
 return 0;
}
