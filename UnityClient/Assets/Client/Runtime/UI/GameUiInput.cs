using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;

namespace Mmorpg.Client
{
    internal static class GameUiInput
    {
        private static int textEscapeFrame = -1;

        public static bool CanUseWorldShortcuts()
        {
            if (EventSystem.current == null) return false;
            var input = SelectedTextInput();
            return input == null || !input.isFocused;
        }

        public static bool EscapeAvailableForPanel()
        {
            if (!Application.isFocused || Keyboard.current == null || !Keyboard.current.escapeKey.wasPressedThisFrame ||
                textEscapeFrame == Time.frameCount) return false;

            var input = SelectedTextInput();
            // TMP may already have ended editing this frame while retaining the selected object.
            if (input != null)
            {
                input.DeactivateInputField();
                EventSystem.current.SetSelectedGameObject(null);
                textEscapeFrame = Time.frameCount;
                return false;
            }
            return true;
        }

        private static TMP_InputField SelectedTextInput()
        {
            var selected = EventSystem.current != null ? EventSystem.current.currentSelectedGameObject : null;
            return selected != null ? selected.GetComponentInParent<TMP_InputField>() : null;
        }
    }
}
