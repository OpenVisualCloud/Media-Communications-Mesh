/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #ifndef _JSON_CONTEXT_
 #define _JSON_CONTEXT_
 #include "mesh_dp.h"

 MeshConfig_Video get_video_params(MeshConnection *conn);
 MeshConfig_Audio get_audio_params(MeshConnection *conn);
 
 #endif /* _JSON_CONTEXT_ */
 