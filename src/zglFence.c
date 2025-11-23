/*
 *           ::::::::    :::::::::::    ::::::::    ::::     ::::       :::
 *          :+:    :+:       :+:       :+:    :+:   +:+:+: :+:+:+     :+: :+:
 *          +:+              +:+       +:+          +:+ +:+:+ +:+    +:+   +:+
 *          +#++:++#++       +#+       :#:          +#+  +:+  +#+   +#++:++#++:
 *                 +#+       +#+       +#+   +#+#   +#+       +#+   +#+     +#+
 *          #+#    #+#       #+#       #+#    #+#   #+#       #+#   #+#     #+#
 *           ########    ###########    ########    ###       ###   ###     ###
 *
 *                     S I G M A   T E C H N O L O G Y   G R O U P
 *
 *                                   Public Test Build
 *                               (c) 2017 SIGMA FEDERATION
 *                             <https://sigmaco.org/qwadro/>
 */


#include "zglUtils.h"
#include "zglCommands.h"
#include "zglObjects.h"

_ZGL afxUnit _DpuProcessFenceSignalChain(zglDpu* dpu)
{
    afxError err = { 0 };
    glVmt const* gl = dpu->gl;

    afxUnit finishedCnt = 0;
    afxUnit remaingCnt = 0;

    avxFence fenc;
    AFX_ITERATE_CHAIN_B2F(fenc, onSignalChain, &dpu->fenceSignalChain)
    {
        GLsync glHandle = AfxLoadAtomPtr(&fenc->glHandleAtom);

        // To block all CPU operations until a sync object is signaled, you call this function:
        GLenum rslt = gl->ClientWaitSync(glHandle, GL_SYNC_FLUSH_COMMANDS_BIT, /*timeout*/0); _ZglThrowErrorOccuried();

        /*
            This function will not return until one of two things happens: the sync​ object parameter becomes signaled, or a number of nanoseconds greater than or equal to the timeout​ parameter passes.
            If timeout​ is zero, the function will simply check to see if the sync object is signaled and return immediately.
            Note that the fact that timeout​ is in nanoseconds does not imply that this function has true nanosecond granularity in its timeout; you are only guaranteed that at least that much time will pass.
        */

        switch (rslt)
        {
        case GL_ALREADY_SIGNALED:
        {
            // the sync object was signaled before the function was called.
            AfxPopLink(&fenc->onSignalChain);
            AfxStoreAtom32(&fenc->m.signaled, 1);
            SetEvent(fenc->hEventW32);
            ++finishedCnt;
            break;
        }
        case GL_CONDITION_SATISFIED:
        {
            // the sync object was signaled within the given timeout period.
            AfxPopLink(&fenc->onSignalChain);
            AfxStoreAtom32(&fenc->m.signaled, 1);
            SetEvent(fenc->hEventW32);
            ++finishedCnt;
            break;
        }
        case GL_TIMEOUT_EXPIRED:
        {
            // the sync object did not signal within the given timeout period.
            ++remaingCnt;
            break;
        }
        case GL_WAIT_FAILED:
        default:
        {
            // If an OpenGL Error occurred, then GL_WAIT_FAILED will be returned in addition to raising an error.
            AfxPopLink(&fenc->onSignalChain);
            break;
        }
        }
    }
    return remaingCnt;
}

_ZGL afxError _DpuWaitForFence(zglDpu* dpu, avxFence fenc)
{
    afxError err = { 0 };
    glVmt const* gl = dpu->gl;

    GLsync glHandle = AfxLoadAtomPtr(&fenc->glHandleAtom);

    if (glHandle)
    {
        AFX_ASSERT(gl->IsSync(glHandle));
        gl->WaitSync(glHandle, GL_NONE, GL_TIMEOUT_IGNORED);


        /*
            There is another function for waiting on sync objects:

            void glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)

            Recall the discussion in the article on Synchronization about the difference between the GPU's command queue and the driver's internal command buffer.
            What glWaitSync does is prevent the driver from adding any commands to the GPU's command queue until this sync object is signaled.
            This function does not halt the CPU.

            The driver will still put commands in its internal buffer. But none of them will be seen by the GPU until this sync object is signaled.

            You need to ensure that the sync object is in the GPU's command queue.
            If you don't, then you may create an infinite loop.
            Since glWaitSync prevents the driver from adding any commands to the GPU command queue, this would include the sync object itself if it has not yet been added to the queue.
            This function does not take the GL_SYNC_FLUSH_COMMANDS_BIT, so you have to do it with a manual glFlush call.


        */
    }
    return err;
}

_ZGL afxError _DpuSignalFence(zglDpu* dpu, avxFence fenc)
{
    afxError err = { 0 };
    glVmt const* gl = dpu->gl;

    GLsync glHandle = gl->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    AFX_ASSERT(gl->IsSync(glHandle));
    glHandle = AfxExchangeAtomPtr(&fenc->glHandleAtom, glHandle);
    if (glHandle)
    {
        AFX_ASSERT(gl->IsSync(glHandle));
        //AFX_ASSERT(fenc->glHandle == glHandle);
        gl->DeleteSync(glHandle);
    }
    AfxPushLink(&fenc->onSignalChain, &dpu->fenceSignalChain);
    return err;
}

_ZGL afxError _ZglResetFence__(zglDpu* dpu, avxFence fenc)
{
    afxError err = { 0 };
    AfxStoreAtom32(&fenc->m.signaled, 0);
    ResetEvent(fenc->hEventW32);
    return err;
}

_ZGL afxError _ZglWaitFenc___(afxDrawSystem dsys, afxUnit64 timeout, afxBool waitAll, afxUnit cnt, avxFence const fences[], afxUnit64 const values[])
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_FENC, cnt, fences);
    //afxDrawSystem dsys = (void*)AvxGetFenceHost(fences[0]);
    afxUnit txuIdx = 0;
    glVmt const* gl = NIL;// &ddev->idd->dpus[txuIdx].gl;

    afxClock startClock, currClock;
    AfxGetClock(&startClock);

    do
    {

        for (afxUnit i = 0; i < cnt; i++)
        {
            avxFence fenc = fences[i];
            AFX_ASSERT_OBJECTS(afxFcc_FENC, 1, fenc);

            // To block all CPU operations until a sync object is signaled, you call this function:
            GLenum rslt = gl->ClientWaitSync(fenc->glHandle, GL_SYNC_FLUSH_COMMANDS_BIT, /*timeout*/0); _ZglThrowErrorOccuried();

            /*
                This function will not return until one of two things happens: the sync​ object parameter becomes signaled, or a number of nanoseconds greater than or equal to the timeout​ parameter passes.
                If timeout​ is zero, the function will simply check to see if the sync object is signaled and return immediately.
                Note that the fact that timeout​ is in nanoseconds does not imply that this function has true nanosecond granularity in its timeout; you are only guaranteed that at least that much time will pass.
            */

            switch (rslt)
            {
            case GL_ALREADY_SIGNALED:
            {
                // the sync object was signaled before the function was called.
                AfxStoreAtom32(&fenc->m.signaled, 1);
                SetEvent(fenc->hEventW32);
                break;
            }
            case GL_TIMEOUT_EXPIRED:
            {
                // the sync object did not signal within the given timeout period.
                //fenc->m.signaled = FALSE;
                break;
            }
            case GL_CONDITION_SATISFIED:
            {
                // the sync object was signaled within the given timeout period.
                AfxStoreAtom32(&fenc->m.signaled, 1);
                SetEvent(fenc->hEventW32);
                break;
            }
            case GL_WAIT_FAILED:
            default:
            {
                // If an OpenGL Error occurred, then GL_WAIT_FAILED will be returned in addition to raising an error.
                //fenc->m.signaled = FALSE;
                break;
            }
            }

            if (fenc->m.signaled && !waitAll)
                break;
        }

        /*
            There is another function for waiting on sync objects:

            void glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)

            Recall the discussion in the article on Synchronization about the difference between the GPU's command queue and the driver's internal command buffer.
            What glWaitSync does is prevent the driver from adding any commands to the GPU's command queue until this sync object is signaled.
            This function does not halt the CPU.

            The driver will still put commands in its internal buffer. But none of them will be seen by the GPU until this sync object is signaled.

            You need to ensure that the sync object is in the GPU's command queue.
            If you don't, then you may create an infinite loop.
            Since glWaitSync prevents the driver from adding any commands to the GPU command queue, this would include the sync object itself if it has not yet been added to the queue.
            This function does not take the GL_SYNC_FLUSH_COMMANDS_BIT, so you have to do it with a manual glFlush call.


        */

        AfxGetClock(&currClock);
    } while (timeout > AfxGetSecondsElapsed(&startClock, &currClock));
    return err;
}

_ZGL afxError _ZglFencWaitFencOnHostCb(avxFence fenc, afxUnit64 const value, afxUnit64 timeout)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_FENC, 1, &fenc);
    WaitForSingleObject(fenc->hEventW32, (timeout == AFX_TIMEOUT_INFINITE) ? INFINITE : AFX_NANOSECS_PER_MILLISECS(timeout));
    return err;
}

_ZGL afxError _ZglFencResetOnHostCb(avxFence const fenc)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_FENC, 1, &fenc);
    
    AfxStoreAtom32(&fenc->m.signaled, 0);
    ResetEvent(fenc->hEventW32);

    return err;
}

_ZGL afxError _ZglFencSignalOnHostCb(avxFence fenc)
{
    afxError err = { 0 };
    afxInt32 expected = 0;
    //AfxCasAtom32(&fenc->m.signaled, &expected, 1);
    AfxStoreAtom32(&fenc->m.signaled, 1);
    SetEvent(fenc->hEventW32);
    return err;
}

_ZGL _avxDdiFenc _ZGL_DDI_FENC =
{
    .waitCb = _ZglFencWaitFencOnHostCb,
    .resetCb = _ZglFencResetOnHostCb,
    .signalCb = _ZglFencSignalOnHostCb
};

_ZGL afxError _ZglFencDtorCb(avxFence fenc)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_FENC, 1, &fenc);
    afxDrawSystem dsys = (void*)AvxGetFenceHost(fenc);

    if (fenc->hEventW32)
    {
        CloseHandle(fenc->hEventW32);
        fenc->hEventW32 = NULL;
    }

    if (fenc->glHandle)
    {
        _ZglDsysEnqueueDeletion(dsys, 0, GL_SYNC_FENCE, (afxSize)fenc->glHandle);
        fenc->glHandle = 0;
    }

    if (_AVX_FENC_CLASS_CONFIG.dtor(fenc))
        AfxThrowError();

    return err;
}

_ZGL afxError _ZglFencCtorCb(avxFence fenc, void** args, afxUnit invokeNo)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_FENC, 1, &fenc);

    if (_AVX_FENC_CLASS_CONFIG.ctor(fenc, args, invokeNo))
    {
        AfxThrowError();
        return err;
    }

    fenc->m.ddi = &_ZGL_DDI_FENC;

    afxDrawSystem dsys = AfxGetHost(fenc);
    fenc->fencUniqueId = ++dsys->fencUniqueId;

    fenc->glHandle = 0;
    fenc->updFlags = ZGL_UPD_FLAG_DEVICE_INST;
        
    fenc->hEventW32 = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (fenc->m.signaled || fenc->m.value)
        SetEvent(fenc->hEventW32);

    /*
        [in, optional] lpEventAttributes
        A pointer to a SECURITY_ATTRIBUTES structure. If this parameter is NULL, the handle cannot be inherited by child processes.
        The lpSecurityDescriptor member of the structure specifies a security descriptor for the new event. 
        If lpEventAttributes is NULL, the event gets a default security descriptor. 
        The ACLs in the default security descriptor for an event come from the primary or impersonation token of the creator.
        [in] bManualReset
        If this parameter is TRUE, the function creates a manual-reset event object, 
        which requires the use of the ResetEvent function to set the event state to nonsignaled. 
        If this parameter is FALSE, the function creates an auto-reset event object, 
        and the system automatically resets the event state to nonsignaled after a single waiting thread has been released.
        [in] bInitialState
        If this parameter is TRUE, the initial state of the event object is signaled; otherwise, it is nonsignaled.
        [in, optional] lpName
        The name of the event object. The name is limited to MAX_PATH characters. Name comparison is case sensitive.
    */

    if (err && _AVX_FENC_CLASS_CONFIG.dtor(fenc))
        AfxThrowError();

    return err;
}

_ZGL afxError _ZglSemDtorCb(afxSemaphore sem)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_SEM, 1, &sem);

    if (_AfxSemStdImplementation.dtor(sem))
        AfxThrowError();

    return err;
}

_ZGL afxError _ZglSemCtorCb(afxSemaphore sem, void** args, afxUnit invokeNo)
{
    afxError err = { 0 };
    AFX_ASSERT_OBJECTS(afxFcc_SEM, 1, &sem);

    if (_AfxSemStdImplementation.ctor(sem, args, invokeNo))
    {
        AfxThrowError();
        return err;
    }

    if (err && _AfxSemStdImplementation.dtor(sem))
        AfxThrowError();

    return err;
}
