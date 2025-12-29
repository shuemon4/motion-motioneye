/**
 * Protected Route Wrapper
 *
 * Wraps routes that require authentication. If the user is not
 * authenticated, it prompts for login via the LoginModal.
 *
 * Note: Motion's HTTP auth is handled by the browser, so we check
 * the auth endpoint to determine if credentials are valid.
 * If authentication is not configured on the server, all requests succeed.
 */

import { type ReactNode } from 'react'
import { useAuthContext } from '@/contexts/AuthContext'

interface ProtectedRouteProps {
  children: ReactNode
  /** If true, only show protected content when authenticated */
  requireAuth?: boolean
}

export function ProtectedRoute({
  children,
  requireAuth = false,
}: ProtectedRouteProps) {
  const { isAuthenticated, isLoading, authRequired } = useAuthContext()

  // If we're still checking auth status, show loading
  if (isLoading) {
    return (
      <div className="p-6">
        <div className="animate-pulse">
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
        </div>
      </div>
    )
  }

  // If auth is required but user isn't authenticated, show message
  // The LoginModal will be triggered by AuthContext automatically
  if (requireAuth && !isAuthenticated && authRequired) {
    return (
      <div className="p-6">
        <div className="bg-yellow-600/10 border border-yellow-600/30 rounded-lg p-6 text-center">
          <h2 className="text-xl font-bold mb-2">Authentication Required</h2>
          <p className="text-gray-400">
            Please sign in to access this page.
          </p>
        </div>
      </div>
    )
  }

  // Render children normally
  return <>{children}</>
}
