#include "param_store.h"
#include "function.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_flash_ex.h"
#include <string.h>
#include <stddef.h>

#define PARAM_STORE_MAGIC 0x54534D50u
#define PARAM_STORE_VERSION 1u
#define PARAM_STORE_COMMIT_DELAY_MS 3000u

#define PARAM_STORE_PAGE0_ADDR 0x0807F000u
#define PARAM_STORE_PAGE1_ADDR 0x0807F800u

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 0x800u
#endif

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t seq;
    float vset;
    float iset;
    float otp;
    float ocp;
    float ovp;
    uint32_t reserved;
    uint32_t crc;
} ParamRecord;

static uint8_t param_has_valid_data = 0;
static uint32_t param_active_addr = 0;
static uint32_t param_active_seq = 0;
static uint8_t param_pending_commit = 0;
static uint32_t param_last_change_ms = 0;

static uint32_t ParamStore_Crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    for (i = 0; i < length; i++)
    {
        uint32_t j;
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 1u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }
    return ~crc;
}

static uint16_t ParamStore_DataSize(void)
{
    return (uint16_t)(sizeof(ParamRecord) - offsetof(ParamRecord, version) - sizeof(uint32_t));
}

static uint8_t ParamStore_RecordValid(const ParamRecord *record)
{
    uint16_t size_expected;
    uint32_t crc;

    if (record == NULL)
    {
        return 0;
    }
    if (record->magic != PARAM_STORE_MAGIC)
    {
        return 0;
    }
    if (record->version != PARAM_STORE_VERSION)
    {
        return 0;
    }
    size_expected = ParamStore_DataSize();
    if (record->size != size_expected)
    {
        return 0;
    }

    crc = ParamStore_Crc32((const uint8_t *)&record->version, record->size);
    return (crc == record->crc) ? 1u : 0u;
}

static const ParamRecord *ParamStore_ReadRecord(uint32_t addr)
{
    return (const ParamRecord *)addr;
}

static uint32_t ParamStore_PageFromAddress(uint32_t address)
{
    return (address - FLASH_BASE) / FLASH_PAGE_SIZE;
}

static uint8_t ParamStore_WriteRecord(uint32_t address, const ParamRecord *record)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    uint32_t offset = 0;
    uint64_t data64 = 0;
    uint8_t status = 0;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return 0;
    }

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Page = ParamStore_PageFromAddress(address);
    erase.NbPages = 1;
    erase.Banks = FLASH_BANK_1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0;
    }

    for (offset = 0; offset < sizeof(ParamRecord); offset += sizeof(uint64_t))
    {
        memcpy(&data64, ((const uint8_t *)record) + offset, sizeof(uint64_t));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + offset, data64) != HAL_OK)
        {
            status = 0;
            break;
        }
        status = 1;
    }

    HAL_FLASH_Lock();
    return status;
}

static void ParamStore_ApplyRecord(const ParamRecord *record)
{
    SET_Value.Vout = record->vset;
    SET_Value.Iout = record->iset;
    MAX_OTP_VAL = record->otp;
    MAX_VOUT_OCP_VAL = record->ocp;
    MAX_VOUT_OVP_VAL = record->ovp;
    Power_ApplySetpoints();
}

uint8_t ParamStore_HasValidData(void)
{
    return param_has_valid_data;
}

void ParamStore_Init(void)
{
    const ParamRecord *rec0 = ParamStore_ReadRecord(PARAM_STORE_PAGE0_ADDR);
    const ParamRecord *rec1 = ParamStore_ReadRecord(PARAM_STORE_PAGE1_ADDR);
    uint8_t valid0 = ParamStore_RecordValid(rec0);
    uint8_t valid1 = ParamStore_RecordValid(rec1);

    if (valid0 && valid1)
    {
        if (rec0->seq >= rec1->seq)
        {
            param_active_addr = PARAM_STORE_PAGE0_ADDR;
            param_active_seq = rec0->seq;
            ParamStore_ApplyRecord(rec0);
        }
        else
        {
            param_active_addr = PARAM_STORE_PAGE1_ADDR;
            param_active_seq = rec1->seq;
            ParamStore_ApplyRecord(rec1);
        }
        param_has_valid_data = 1;
    }
    else if (valid0)
    {
        param_active_addr = PARAM_STORE_PAGE0_ADDR;
        param_active_seq = rec0->seq;
        ParamStore_ApplyRecord(rec0);
        param_has_valid_data = 1;
    }
    else if (valid1)
    {
        param_active_addr = PARAM_STORE_PAGE1_ADDR;
        param_active_seq = rec1->seq;
        ParamStore_ApplyRecord(rec1);
        param_has_valid_data = 1;
    }
    else
    {
        param_active_addr = 0;
        param_active_seq = 0;
        param_has_valid_data = 0;
    }

    param_pending_commit = 0;
    param_last_change_ms = HAL_GetTick();
}

void ParamStore_RequestCommit(void)
{
    param_pending_commit = 1;
    param_last_change_ms = HAL_GetTick();
}

void ParamStore_Tick(void)
{
    uint32_t now_ms;
    uint32_t target_addr;
    ParamRecord record;

    if (!param_pending_commit)
    {
        return;
    }
    now_ms = HAL_GetTick();
    if ((now_ms - param_last_change_ms) < PARAM_STORE_COMMIT_DELAY_MS)
    {
        return;
    }
    if ((DF.SMFlag != Wait) || (DF.OUTPUT_Flag != 0))
    {
        return;
    }

    memset(&record, 0, sizeof(record));
    record.magic = PARAM_STORE_MAGIC;
    record.version = PARAM_STORE_VERSION;
    record.size = ParamStore_DataSize();
    record.seq = param_active_seq + 1u;
    record.vset = SET_Value.Vout;
    record.iset = SET_Value.Iout;
    record.otp = MAX_OTP_VAL;
    record.ocp = MAX_VOUT_OCP_VAL;
    record.ovp = MAX_VOUT_OVP_VAL;
    record.reserved = 0;
    record.crc = ParamStore_Crc32((const uint8_t *)&record.version, record.size);

    if (!param_has_valid_data)
    {
        target_addr = PARAM_STORE_PAGE0_ADDR;
    }
    else
    {
        target_addr = (param_active_addr == PARAM_STORE_PAGE0_ADDR) ? PARAM_STORE_PAGE1_ADDR : PARAM_STORE_PAGE0_ADDR;
    }

    if (ParamStore_WriteRecord(target_addr, &record))
    {
        param_active_addr = target_addr;
        param_active_seq = record.seq;
        param_has_valid_data = 1;
        param_pending_commit = 0;
    }
}
