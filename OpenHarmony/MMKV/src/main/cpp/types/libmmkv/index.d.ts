export const initialize: (rootDir: string, cacheDir: string, logLevel?: number) => string;
export const version: () => string;
export const getDefaultMMKV: (mode: number, cryptKey?: string) => BigInt;
export const encodeBool: (handle: BigInt, key: string, value: boolean, expiration?: number) => boolean;
export const decodeBool: (handle: BigInt, key: string, defaultValue: boolean) => boolean;
