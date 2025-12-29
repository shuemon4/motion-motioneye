/**
 * Authentication Context for React UI
 *
 * Provides authentication state management for the application.
 * Motion uses HTTP Basic/Digest authentication via webcontrol_authentication.
 * This context tracks authentication status and handles login prompts.
 */

import {
  createContext,
  useContext,
  useState,
  useCallback,
  useEffect,
  type ReactNode,
} from 'react'
import { useAuth } from '@/api/queries'

interface AuthState {
  isAuthenticated: boolean
  isLoading: boolean
  authMethod: string | null
  authRequired: boolean
}

interface AuthContextValue extends AuthState {
  /** Show login modal for user authentication */
  showLoginModal: () => void
  /** Hide login modal */
  hideLoginModal: () => void
  /** Whether login modal is currently visible */
  isLoginModalVisible: boolean
  /** Called after successful authentication */
  onLoginSuccess: () => void
  /** Set authentication required flag (triggered by 401/403) */
  setAuthRequired: (required: boolean) => void
  /** Check authentication status */
  checkAuth: () => void
}

const AuthContext = createContext<AuthContextValue | null>(null)

interface AuthProviderProps {
  children: ReactNode
}

export function AuthProvider({ children }: AuthProviderProps) {
  const [isLoginModalVisible, setIsLoginModalVisible] = useState(false)
  const [authRequired, setAuthRequired] = useState(false)

  // Query authentication status from backend
  const { data: authData, isLoading, refetch } = useAuth()

  const isAuthenticated = authData?.authenticated ?? false
  const authMethod = authData?.auth_method ?? null

  // Auto-show login modal if auth is required but user isn't authenticated
  useEffect(() => {
    if (authRequired && !isAuthenticated && !isLoading) {
      setIsLoginModalVisible(true)
    }
  }, [authRequired, isAuthenticated, isLoading])

  const showLoginModal = useCallback(() => {
    setIsLoginModalVisible(true)
  }, [])

  const hideLoginModal = useCallback(() => {
    setIsLoginModalVisible(false)
  }, [])

  const onLoginSuccess = useCallback(() => {
    setIsLoginModalVisible(false)
    setAuthRequired(false)
    // Refetch auth status
    refetch()
  }, [refetch])

  const checkAuth = useCallback(() => {
    refetch()
  }, [refetch])

  const value: AuthContextValue = {
    isAuthenticated,
    isLoading,
    authMethod,
    authRequired,
    showLoginModal,
    hideLoginModal,
    isLoginModalVisible,
    onLoginSuccess,
    setAuthRequired,
    checkAuth,
  }

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>
}

/**
 * Hook to access authentication context
 * @throws Error if used outside of AuthProvider
 */
export function useAuthContext(): AuthContextValue {
  const context = useContext(AuthContext)
  if (!context) {
    throw new Error('useAuthContext must be used within an AuthProvider')
  }
  return context
}
