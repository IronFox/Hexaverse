using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;


namespace Consensus
{
    public static class LoadingStage
    {
        public static bool IsLoading { get; private set; }

        public static event Action  OnExit;


        public static void Exit()
        {
            Debug.Assert(IsLoading);
            IsLoading = false;
            if (OnExit != null)
                OnExit();
        }

        public static void Enter()
        {
            Debug.Assert(!IsLoading);
            IsLoading = true;
        }

        public static void Abort()
        {
            Debug.Assert(IsLoading);
            IsLoading = false;
        }
    }
}
