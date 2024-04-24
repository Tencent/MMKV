import { MMKV } from '@tencent/mmkv';
import dataPreferences from '@ohos.data.preferences';

export class Baseline {
  private static MMKV_ID = 'baseline';
  private static CryptKey = undefined;
  private m_arrStrings: string[] = [];
  private m_arrKeys: string[] = [];
  private m_arrIntKeys: string[] = [];
  private m_loops = 1000;
  private m_context: Context;
  // private m_formatter: Intl.NumberFormat;

  constructor(context: Context, loops: number) {
    this.m_context = context;
    this.m_loops = loops;

    this.m_arrStrings = new Array(loops);
    this.m_arrKeys = new Array(loops);
    this.m_arrIntKeys = new Array(loops);
    const filename = "mmkv/Android/MMKV/mmkvdemo/src/main/java/com/tencent/mmkvdemo/Baseline.java_";
    for (let index = 0; index < loops; index++) {
      this.m_arrStrings[index] = filename + Math.floor(Math.random() * Number.MAX_SAFE_INTEGER);
      this.m_arrKeys[index] = "str_" + index;
      this.m_arrIntKeys[index] = "int_" + index;
    }
    // this.m_formatter = new Intl.NumberFormat('en-US', { minimumFractionDigits: 1, maximumFractionDigits: 1 });
  }

  private mmkvForTest() {
    return MMKV.mmkvWithID(Baseline.MMKV_ID, MMKV.SINGLE_PROCESS_MODE, Baseline.CryptKey);
    //return MMKV.mmkvWithAshmemID(m_context, MMKV_ID, 65536, MMKV.SINGLE_PROCESS_MODE, CryptKey);
  }

  private preferenceForTest() {
    return dataPreferences.getPreferencesSync(this.m_context, { name: Baseline.MMKV_ID });
  }

  public mmkvBaselineTest() {
    this.mmkvBatchWriteInt();
    this.mmkvBatchReadInt();
    this.mmkvBatchWriteString();
    this.mmkvBatchReadString();

    //this.mmkvBatchDeleteString();
    // let mmkv = this.mmkvForTest();
    //mmkv.trim();
    // mmkv.clearMemoryCache();
    // mmkv.totalSize();
  }

  private mmkvBatchWriteInt() {
    let r = Math.random();
    let startTime = Date.now();

    let mmkv = this.mmkvForTest();
    for (let index = 0; index < this.m_loops; index++) {
      let tmp = Math.floor(r * (Number.MAX_SAFE_INTEGER + 1));
      let key = this.m_arrIntKeys[index];
      mmkv.encodeInt32(key, tmp);
    }
    let endTime = Date.now() - startTime;
    console.log("MMKV write int: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private mmkvBatchReadInt() {
    const startTime = Date.now();

    const mmkv = this.mmkvForTest();
    for (let index = 0; index < this.m_loops; index++) {
      const key = this.m_arrIntKeys[index];
      const tmp = mmkv.decodeInt32(key);
    }
    const endTime = Date.now() - startTime;
    console.log("MMKV read int: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private mmkvBatchWriteString() {
    const startTime = Date.now();

    const mmkv = this.mmkvForTest();
    for (let index = 0; index < this.m_loops; index++) {
      const valueStr = this.m_arrStrings[index];
      const strKey = this.m_arrKeys[index];
      mmkv.encodeString(strKey, valueStr);
    }
    const endTime = Date.now() - startTime;
    console.log("MMKV write String: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private mmkvBatchReadString() {
    const startTime = Date.now();

    const mmkv = this.mmkvForTest();
    for (let index = 0; index < this.m_loops; index++) {
      const strKey = this.m_arrKeys[index];
      const tmpStr = mmkv.decodeString(strKey);
    }
    const endTime = Date.now() - startTime;
    console.log("MMKV read String: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  public preferencesBaselineTest() {
    this.preferencesBatchWriteInt();
    this.preferencesBatchReadInt();
    this.preferencesBatchWriteString();
    this.preferencesBatchReadString();
  }

  private preferencesBatchWriteInt() {
    let r = Math.random();
    let startTime = Date.now();

    let sp = this.preferenceForTest();
    for (let index = 0; index < this.m_loops; index++) {
      let tmp = Math.floor(r * (Number.MAX_SAFE_INTEGER + 1));
      let key = this.m_arrIntKeys[index];
      sp.putSync(key, tmp);
    }
    sp.flush();

    let endTime = Date.now() - startTime;
    console.log("Preferences write int: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private preferencesBatchReadInt() {
    const startTime = Date.now();

    const sp = this.preferenceForTest();
    for (let index = 0; index < this.m_loops; index++) {
      let key = this.m_arrIntKeys[index];
      let tmp = sp.getSync(key, 0) as number;
    }
    sp.flush();

    let endTime = Date.now() - startTime;
    console.log("Preferences read int: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private preferencesBatchWriteString() {
    const startTime = Date.now();

    const sp = this.preferenceForTest();
    for (let index = 0; index < this.m_loops; index++) {
      const valueStr = this.m_arrStrings[index];
      const strKey = this.m_arrKeys[index];
      sp.putSync(strKey, valueStr);
    }
    sp.flush();

    let endTime = Date.now() - startTime;
    console.log("Preferences write String: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }

  private preferencesBatchReadString() {
    const startTime = Date.now();

    const sp = this.preferenceForTest();
    for (let index = 0; index < this.m_loops; index++) {
      const strKey = this.m_arrKeys[index];
      const tmpStr = sp.getSync(strKey, null) as string;
    }
    sp.flush();

    let endTime = Date.now() - startTime;
    console.log("Preferences read String: loop[" + this.m_loops + "]: " + endTime.toFixed(2) + " ms");
  }
}
