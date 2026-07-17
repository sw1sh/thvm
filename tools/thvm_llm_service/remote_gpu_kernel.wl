(* Remote GPU kernel on a Prime Intellect Wolfram-CUDA pod, via RemoteEvaluate.
   Setup that made it work (all three non-obvious):
   1. ~/.ssh/config needs a Host entry for the pod IP (IdentityFile + Port) -- WL's ssh
      ignores a key passed only in the config association otherwise. SshIdentity below
      also passes -i, which is what actually lands (see the ssh command WL builds).
   2. The pod licenses ONLY the on-demand entitlement, and a raw `wolfram -wstp` kernel
      cannot take it. The kernel command must be
        WolframKernel -pwfile !cloudlm.wolfram.com -entitlement <ID>
      (the cloud-license-manager pwfile is the half that `wolfram -entitlement` alone
      lacks; it's exactly how wolframscript launches its kernel).
   3. RemoteKernelObject is obsolete in 15.0 -- use KernelConfiguration.
   NOTE: each RemoteEvaluate[conf, expr] launches a FRESH kernel (~6-7 s) and does not
   persist state. For multi-step work put it all in one call: re[{step1; step2; ...}].
   The re[] wrapper MUST be HoldFirst or the argument evaluates locally first. *)

$thvmPod = KernelConfiguration["ssh://<POD_IP>",
  "Username" -> "root", "Port" -> <SSH_PORT>,
  "SshIdentity" -> FileNameJoin[{$HomeDirectory, ".ssh", "<YOUR_KEY>"}],
  "KernelCommand" -> "WolframKernel -pwfile !cloudlm.wolfram.com -entitlement <ENTITLEMENT_ID>"];

ClearAll[re]; SetAttributes[re, HoldFirst];
re[expr_] := RemoteEvaluate[$thvmPod, Unevaluated[expr]]

(* examples:
   re[{$SystemID, $MachineName}]                                     (* -> {Linux-x86-64, ...} : proves remote *)
   re[NetInitialize[LinearLayer[4,"Input"->3]][{1.,2.,3.}, TargetDevice->"GPU"]]
   re[img = ResourceFunction["StableDiffusionSynthesize"]["a fox", TargetDevice->"GPU", MaxIterations->25]]
   NOTE StableDiffusionSynthesize with TargetDevice->"GPU" FAILS on macOS Metal in 15.0
   ("NetChain: Computation of intermediate NetPort outputs is not supported for the provided
   TargetDevice setting"); it works on CPU and here on the pod's CUDA. *)
