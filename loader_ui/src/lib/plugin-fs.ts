import { ipcInvoke, apiGET, apiPOST } from './ipc'

/**
 * Filesystem error type
 */
export type FsError = {
  code:
  | 'not_found'
  | 'permission_denied'
  | 'invalid_path'
  | 'io_error'
  | 'unknown'
  message: string
}

function sanitizePath(path: string): string {
  // Check empty path
  if (!path || typeof path !== 'string') {
    throw {
      code: 'invalid_path',
      message: 'Path cannot be null or empty'
    } as FsError
  }
  // Deny path traversal
  if (path.includes('..')) {
    throw {
      code: 'invalid_path',
      message: 'Path traversal is not allowed'
    } as FsError
  }
  return path.replace(/\\/g, '/')
}

/**
 * Check if file or directory exists.
 * @param path File or directory path
 * @returns A promise that resolves to true if exists, false otherwise
 * @throws {FsError}
 */
export function exists(path: string): Promise<boolean> {
  path = sanitizePath(path)
  return ipcInvoke<boolean>('fs:exists', path)
}

/**
 * Read entire file as string.
 * @param path File path
 * @param encoding File encoding, default is 'utf8'
 * @returns A promise that resolves to file content as string
 * @throws {FsError}
 */
export function readFile(
  path: string,
  encoding?: 'utf8'
): Promise<string>

/**
 * Read entire file as binary.
 * @param path File path
 * @param encoding File encoding, must be 'binary'
 * @returns A promise that resolves to file content as Uint8Array
 * @throws {FsError}
 */
export function readFile(
  path: string,
  encoding: 'binary'
): Promise<ArrayBuffer>

export async function readFile(
  path: string,
  encoding: 'utf8' | 'binary' = 'utf8'
): Promise<string | ArrayBuffer> {
  path = sanitizePath(path)
  const res = await apiGET(
    `/api/fs/read?path=${encodeURIComponent(path)}&encoding=${encoding}`
  )

  if (!res.ok) {
    throw new Error(await res.text())
  }

  if (encoding === 'utf8') {
    return await res.text()
  } else {
    return await res.arrayBuffer()
  }
}

/**
 * Write data to file.
 * @param path File path
 * @param data Data to write
 * @returns A promise that resolves when the write is complete
 * @throws {FsError}
 */
export async function writeFile(
  path: string,
  data: string | ArrayBuffer
): Promise<void> {
  path = sanitizePath(path)
  const encoding = typeof data === 'string' ? 'utf8' : 'binary'
  await apiPOST(
    `/api/fs/write?path=${encodeURIComponent(path)}&encoding=${encoding}`,
    data
  )
}

/**
 * Read directory contents.
 * @param path Directory path
 * @param options Options object
 * @returns A promise that resolves to an array of file and directory names
 * @throws {FsError}
 */
export function readdir(
  path: string,
  options?: {
    recursive: false
  }
): Promise<string[]> {
  path = sanitizePath(path)
  options = options || { recursive: false }
  return ipcInvoke<string[]>('fs:readdir', path, options)
}

/**
 * Get file or directory statistics.
 * @param path File or directory path
 * @returns A promise that resolves to an object containing statistics
 * @throws {FsError}
 */
export function stat(
  path: string
): Promise<{
  isFile: boolean
  isDirectory: boolean
  size: number
}> {
  path = sanitizePath(path)
  return ipcInvoke<{
    isFile: boolean
    isDirectory: boolean
    size: number
  }>('fs:stat', path)
}

/**
 * Remove file or directory.
 * @param path File or directory path
 * @returns A promise that resolves when the removal is complete
 * @throws {FsError}
 */
export function rm(
  path: string
): Promise<void> {
  path = sanitizePath(path)
  return ipcInvoke<void>('fs:rm', path)
}

export default {
  exists,
  readFile,
  writeFile,
  readdir,
  stat,
  rm
}