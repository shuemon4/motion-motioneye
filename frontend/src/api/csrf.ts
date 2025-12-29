/**
 * CSRF Token Management
 * Token is obtained from config response and cached in memory.
 * Automatically refreshed on 403 errors.
 */

let csrfToken: string | null = null;

export function setCsrfToken(token: string): void {
  csrfToken = token;
}

export function getCsrfToken(): string | null {
  return csrfToken;
}

export function invalidateCsrfToken(): void {
  csrfToken = null;
}
