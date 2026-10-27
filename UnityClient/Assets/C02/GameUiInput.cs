using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;

namespace C02
{
    internal static class GameUiInput
    {
        private static int textEscapeFrame = -1;

        public static bool EscapeAvailableForPanel()
        {
            if (!Application.isFocused || Keyboard.current == null || !Keyboard.current.escapeKey.wasPressedThisFrame ||
                textEscapeFrame == Time.frameCount) return false;

            var selected = EventSystem.current != null ? EventSystem.current.currentSelectedGameObject : null;
            var input = selected != null ? selected.GetComponentInParent<TMP_InputField>() : null;
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
    }
}
