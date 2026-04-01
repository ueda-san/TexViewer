// https://stackoverflow.com/questions/76578003/how-can-i-change-the-pointer-of-my-cursor-in-winui3
// https://learn.microsoft.com/en-us/answers/questions/1373806/how-to-change-the-pointer-cursor-in-winui
using Microsoft.UI.Input;
using Microsoft.UI.Xaml.Controls;

namespace TexViewer
{
    public class CustomGrid : Grid
    {
        bool busy = false;
        public InputCursor InputCursor {
            get => ProtectedCursor;
            set { if (!busy) ProtectedCursor = value; }
        }
        public void SetBusy(bool flag) {
            busy = flag;
            ProtectedCursor = busy?
                InputSystemCursor.Create(InputSystemCursorShape.Wait):
                InputSystemCursor.Create(InputSystemCursorShape.Arrow);
        }
        public bool IsBusy(){
            return busy;
        }
    }
}
